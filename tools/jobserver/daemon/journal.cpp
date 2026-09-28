#include "journal.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace jobserver {
auto event_name(EventKind const kind) -> std::string {
    switch (kind) {
        case EventKind::daemon_started:
            return "daemon_started";
        case EventKind::daemon_stopped:
            return "daemon_stopped";
        case EventKind::client_connected:
            return "client_connected";
        case EventKind::client_disconnected:
            return "client_disconnected";
        case EventKind::command_received:
            return "command_received";
        case EventKind::lease_requested:
            return "lease_requested";
        case EventKind::lease_queued:
            return "lease_queued";
        case EventKind::blocker_changed:
            return "blocker_changed";
        case EventKind::lease_granted:
            return "lease_granted";
        case EventKind::command_started:
            return "command_started";
        case EventKind::command_completed:
            return "command_completed";
        case EventKind::lease_released:
            return "lease_released";
        case EventKind::request_cancelled:
            return "request_cancelled";
        case EventKind::exclusive_queued:
            return "exclusive_queued";
        case EventKind::exclusive_granted:
            return "exclusive_granted";
        case EventKind::exclusive_released:
            return "exclusive_released";
        case EventKind::server_health:
            return "server_health";
        case EventKind::protocol_error:
            return "protocol_error";
    }
    return "unknown";
}

RotatingLog::RotatingLog(std::filesystem::path path,
                         std::size_t const segment_bytes,
                         std::size_t const segments)
    : path_{std::move(path)}
    , segment_bytes_{segment_bytes}
    , segments_{segments} {
    if (path_.empty()) {
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(path_.parent_path(), error);
    if (error) {
        std::cerr << "Cannot create journal directory: " << error.message() << '\n';
        return;
    }
    auto const size{std::filesystem::file_size(path_, error)};
    written_ = error ? 0 : static_cast<std::size_t>(size);
    output_.open(path_, std::ios::app | std::ios::binary);
    if (output_ && written_ != 0) {
        std::ifstream tail{path_, std::ios::binary};
        tail.seekg(-1, std::ios::end);
        char last{};
        if (tail.get(last) && last != '\n') {
            output_.put('\n');
            output_.flush();
            ++written_;
        }
    }
    if (!output_) {
        std::cerr << "Cannot open journal " << path_ << '\n';
    }
}
auto RotatingLog::paths() const -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> result;
    if (path_.empty()) {
        return result;
    }
    for (auto index{segments_}; index > 1; --index) {
        result.emplace_back(path_.wstring() + L"." + std::to_wstring(index - 1));
    }
    result.push_back(path_);
    return result;
}
void RotatingLog::write(std::string const& line) {
    if (!output_) {
        return;
    }
    if (written_ != 0 && written_ + line.size() + 1 > segment_bytes_) {
        output_.close();
        auto const files{paths()};
        std::error_code error;
        std::filesystem::remove(files.front(), error);
        for (std::size_t index{1}; index < files.size(); ++index) {
            error.clear();
            if (std::filesystem::exists(files[index], error)) {
                std::filesystem::rename(files[index], files[index - 1], error);
                if (error) {
                    std::cerr << "Journal rotation failed: " << error.message() << '\n';
                }
            }
        }
        output_.open(path_, std::ios::trunc | std::ios::binary);
        written_ = 0;
    }
    output_ << line << '\n';
    output_.flush();
    written_ += line.size() + 1;
}

Journal::Journal(std::filesystem::path directory,
                 std::size_t const capacity,
                 std::size_t const segment_bytes)
    : capacity_{std::max<std::size_t>(capacity, 1)}
    , timestamps_(capacity_)
    , kinds_(capacity_)
    , clients_(capacity_)
    , commands_(capacity_)
    , leases_(capacity_)
    , gates_(capacity_)
    , related_(capacity_)
    , values_(capacity_)
    , payloads_(capacity_)
    , disk_{directory.empty() ? directory : directory / "events.jsonl", segment_bytes}
    , human_{directory.empty() ? directory : directory / "jobserverd.log", 16U * 1024U * 1024U} {
    load();
}
void Journal::evict() {
    auto const id{payloads_[first_].value};
    if (id != 0) {
        auto found{strings_.find(id)};
        if (--found->second.references == 0) {
            payload_bytes_ -= found->second.text.size();
            interned_.erase(found->second.text);
            strings_.erase(found);
        }
    }
    first_ = (first_ + 1) % capacity_;
    --size_;
}
void Journal::insert(Event event, std::string const& payload) {
    while (size_ != 0 &&
           (size_ == capacity_ || payload_bytes_ + payload.size() > payload_capacity)) {
        evict();
    }
    if (!payload.empty()) {
        auto found{interned_.find(payload)};
        if (found == interned_.end()) {
            event.payload = PayloadId{next_payload_++};
            interned_.emplace(payload, event.payload);
            strings_.emplace(event.payload.value, Payload{payload, 0});
            payload_bytes_ += payload.size();
        } else {
            event.payload = found->second;
        }
        ++strings_.at(event.payload.value).references;
    }
    auto const index{(first_ + size_++) % capacity_};
    timestamps_[index] = event.timestamp;
    kinds_[index] = event.kind;
    clients_[index] = event.client;
    commands_[index] = event.command;
    leases_[index] = event.lease;
    gates_[index] = event.gate;
    related_[index] = event.related;
    values_[index] = event.value;
    payloads_[index] = event.payload;
}
auto Journal::row(std::size_t const index) const -> Event {
    return {timestamps_[index],
            kinds_[index],
            clients_[index],
            commands_[index],
            leases_[index],
            gates_[index],
            related_[index],
            values_[index],
            payloads_[index]};
}
void Journal::append(Event event, std::string payload) {
    std::scoped_lock lock{mutex_};
    event.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
    payload.resize(std::min<std::size_t>(payload.size(), 32U * 1024U));
    insert(event, payload);
    auto const payload_id{payloads_[(first_ + size_ - 1) % capacity_].value};
    // Rotate a payload definition together with its row, so each segment stands alone.
    auto const definition{
        payload_id ? nlohmann::json{{"payload", payload_id}, {"text", payload}}.dump() + "\n"
                   : std::string{}};
    disk_.write(definition + nlohmann::json::array({event.timestamp,
                                                    static_cast<unsigned>(event.kind),
                                                    event.client.value,
                                                    event.command.value,
                                                    event.lease.value,
                                                    event.gate.value,
                                                    event.related.value,
                                                    event.value,
                                                    payload_id})
                                 .dump());
    human_.write(std::to_string(event.timestamp) + " " + event_name(event.kind) +
                 " client=" + std::to_string(event.client.value) +
                 " command=" + std::to_string(event.command.value) +
                 " lease=" + std::to_string(event.lease.value) + " " + payload);
}
void Journal::load() {
    for (auto const& path : disk_.paths()) {
        std::ifstream input{path, std::ios::binary};
        std::string line;
        std::string payload;
        std::uint64_t payload_id{};
        while (std::getline(input, line)) {
            try {
                auto const json = nlohmann::json::parse(line);
                if (json.is_object()) {
                    payload_id = json.at("payload").get<std::uint64_t>();
                    payload = json.at("text").get<std::string>();
                    continue;
                }
                if (!json.is_array() || json.size() != 9 ||
                    json[1].get<unsigned>() > static_cast<unsigned>(EventKind::protocol_error)) {
                    continue;
                }
                insert({json[0].get<std::int64_t>(),
                        static_cast<EventKind>(json[1].get<unsigned>()),
                        ClientId{json[2].get<std::uint64_t>()},
                        CommandId{json[3].get<std::uint64_t>()},
                        LeaseId{json[4].get<std::uint64_t>()},
                        GateId{json[5].get<std::uint32_t>()},
                        LeaseId{json[6].get<std::uint64_t>()},
                        json[7].get<std::int64_t>()},
                       json[8].get<std::uint64_t>() != 0 &&
                               json[8].get<std::uint64_t>() == payload_id
                           ? payload
                           : std::string{});
            } catch (nlohmann::json::exception const&) { /* An interrupted final row is diagnostic
                                                            only. */
            }
        }
    }
}
auto Journal::trace(TraceFilter const& filter) const -> nlohmann::json {
    std::scoped_lock lock{mutex_};
    auto result = nlohmann::json::array();
    std::size_t bytes{};
    for (auto offset{size_};
         offset > 0 && result.size() < std::min<std::size_t>(filter.limit, 1000);
         --offset) {
        auto const event{row((first_ + offset - 1) % capacity_)};
        if ((filter.client.value && event.client != filter.client) ||
            (filter.command.value && event.command != filter.command) ||
            (filter.lease.value && event.lease != filter.lease) ||
            (filter.gate.value && event.gate != filter.gate) ||
            (filter.kind && event.kind != *filter.kind)) {
            continue;
        }
        auto const payload{event.payload.value ? strings_.at(event.payload.value).text
                                               : std::string{}};
        nlohmann::json item{{"timestamp_us", event.timestamp},
                            {"event", event_name(event.kind)},
                            {"client", event.client.value},
                            {"command", event.command.value},
                            {"lease", event.lease.value},
                            {"gate", event.gate.value},
                            {"related", event.related.value},
                            {"value", event.value},
                            {"payload_id", event.payload.value},
                            {"payload", payload}};
        bytes += item.dump().size();
        if (bytes > 768U * 1024U) {
            break;
        }
        result.push_back(std::move(item));
    }
    std::reverse(result.begin(), result.end());
    return result;
}
}
