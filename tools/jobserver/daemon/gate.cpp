#include "gate.hpp"

#include <algorithm>
#include <chrono>
#include <set>

namespace jobserver {
GateQueue::GateQueue(Journal& journal)
    : journal_{journal}
    , next_id_{static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                              std::chrono::system_clock::now().time_since_epoch())
                                              .count())} {}
auto GateQueue::connect() -> ClientId {
    auto const id{ClientId{next_id_++}};
    clients_.push_back(id);
    journal_.append({.kind = EventKind::client_connected, .client = id});
    return id;
}
void GateQueue::disconnect(ClientId const client) {
    if (auto const* entry{find(client)}) {
        remove(client, entry->lease, !entry->granted);
    }
    std::erase(clients_, client);
    journal_.append({.kind = EventKind::client_disconnected, .client = client});
}
auto GateQueue::acquire(ClientId const client,
                        std::vector<GateClaim> claims,
                        nlohmann::json metadata) -> std::expected<Admission, Error> {
    if (std::ranges::find(clients_, client) == clients_.end() || find(client)) {
        return std::unexpected(
            Error{"client_busy", "A session may have only one pending or granted command"});
    }
    std::set<std::string> names;
    if (claims.empty() || claims.size() > 8) {
        return std::unexpected(Error{"invalid_gates", "Supply between one and eight named gates"});
    }
    for (auto const& claim : claims) {
        if (claim.name.empty() || claim.name.size() > 256 || !names.insert(claim.name).second) {
            return std::unexpected(
                Error{"invalid_gates", "Gate names must be nonempty, bounded and unique"});
        }
    }
    for (auto const& claim : claims) {
        if (!gates_.contains(claim.name)) {
            gates_.emplace(claim.name, GateId{static_cast<std::uint32_t>(gates_.size() + 1)});
        }
    }
    entries_.push_back({client,
                        CommandId{next_id_++},
                        LeaseId{next_id_++},
                        std::move(claims),
                        std::move(metadata)});
    auto const& entry{entries_.back()};
    journal_.append({.kind = EventKind::command_received,
                     .client = client,
                     .command = entry.command,
                     .lease = entry.lease},
                    entry.metadata.dump());
    record(EventKind::lease_requested, entry);
    admit();
    if (!entries_.back().granted) {
        record(EventKind::lease_queued, entries_.back());
        exclusive_event(EventKind::exclusive_queued, entries_.back());
    }
    return entries_.back();
}
auto GateQueue::find(ClientId const client) const -> Admission const* {
    auto const found{std::ranges::find(entries_, client, &Admission::client)};
    return found == entries_.end() ? nullptr : &*found;
}
void GateQueue::record(EventKind const kind, Admission const& entry, std::int64_t const value) {
    for (auto const& claim : entry.claims) {
        journal_.append({.kind = kind,
                         .client = entry.client,
                         .command = entry.command,
                         .lease = entry.lease,
                         .gate = gates_.at(claim.name),
                         .value = value},
                        claim.name + "/" +
                            (claim.mode == LeaseMode::exclusive ? "exclusive" : "shared"));
    }
}
void GateQueue::exclusive_event(EventKind const kind, Admission const& entry) {
    for (auto const& claim : entry.claims) {
        if (claim.mode == LeaseMode::exclusive) {
            journal_.append({.kind = kind,
                             .client = entry.client,
                             .command = entry.command,
                             .lease = entry.lease,
                             .gate = gates_.at(claim.name)},
                            claim.name);
        }
    }
}
auto GateQueue::conflicts(Admission const& left, Admission const& right) const -> bool {
    for (auto const& a : left.claims) {
        for (auto const& b : right.claims) {
            if (a.name == b.name &&
                (a.mode == LeaseMode::exclusive || b.mode == LeaseMode::exclusive)) {
                return true;
            }
        }
    }
    return false;
}
void GateQueue::admit() {
    auto const count{entries_.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto& entry{entries_[index]};
        if (entry.granted) {
            continue;
        }
        std::vector<LeaseId> blockers;
        // An older conflicting waiter reserves its place even before it owns the gate.
        for (std::size_t older{}; older < index; ++older) {
            if (conflicts(entry, entries_[older])) {
                blockers.push_back(entries_[older].lease);
            }
        }
        if (blockers != entry.blockers) {
            entry.blockers = blockers;
            journal_.append({.kind = EventKind::blocker_changed,
                             .client = entry.client,
                             .command = entry.command,
                             .lease = entry.lease,
                             .related = blockers.empty() ? LeaseId{} : blockers.front(),
                             .value = static_cast<std::int64_t>(blockers.size())});
        }
        if (blockers.empty()) {
            entry.granted = true;
            record(EventKind::lease_granted, entry);
            exclusive_event(EventKind::exclusive_granted, entry);
        }
    }
}
void GateQueue::remove(ClientId const client, LeaseId const lease, bool const cancelled) {
    auto const found{std::ranges::find(entries_, lease, &Admission::lease)};
    if (found == entries_.end() || found->client != client) {
        return;
    }
    record(cancelled ? EventKind::request_cancelled : EventKind::lease_released, *found);
    if (!cancelled) {
        exclusive_event(EventKind::exclusive_released, *found);
    }
    entries_.erase(found);
    admit();
}
auto GateQueue::cancel(ClientId const client, LeaseId const lease) -> bool {
    auto const* entry{find(client)};
    if (!entry || entry->lease != lease || entry->started) {
        return false;
    }
    // A grant racing with clear is relinquished before the client starts anything.
    remove(client, lease, !entry->granted);
    return true;
}
auto GateQueue::release(ClientId const client, LeaseId const lease, int const exit_code) -> bool {
    auto const* entry{find(client)};
    if (!entry || entry->lease != lease || !entry->granted) {
        return false;
    }
    journal_.append({.kind = EventKind::command_completed,
                     .client = client,
                     .command = entry->command,
                     .lease = lease,
                     .value = exit_code});
    remove(client, lease, false);
    return true;
}
auto GateQueue::started(ClientId const client, LeaseId const lease, std::uint32_t const pid)
    -> bool {
    auto found{std::ranges::find(entries_, client, &Admission::client)};
    if (found == entries_.end() || found->lease != lease || !found->granted || found->started) {
        return false;
    }
    found->started = true;
    journal_.append({.kind = EventKind::command_started,
                     .client = client,
                     .command = found->command,
                     .lease = lease,
                     .value = pid});
    return true;
}
auto GateQueue::status() const -> nlohmann::json {
    auto jobs = nlohmann::json::array();
    auto clients = nlohmann::json::array();
    auto gates = nlohmann::json::array();
    for (auto const client : clients_) {
        clients.push_back({{"id", client.value}});
    }
    for (auto const& entry : entries_) {
        auto job = entry.metadata;
        job["id"] = std::to_string(entry.lease.value);
        job["lease"] = entry.lease.value;
        job["command"] = entry.command.value;
        job["client"] = entry.client.value;
        job["state"] = entry.granted ? "RUNNING" : "QUEUED";
        job["locally_started"] = entry.started;
        job["claims"] = nlohmann::json::array();
        for (auto const& claim : entry.claims) {
            job["claims"].push_back(
                {{"name", claim.name},
                 {"gate", gates_.at(claim.name).value},
                 {"mode", claim.mode == LeaseMode::exclusive ? "exclusive" : "shared"}});
        }
        job["blockers"] = nlohmann::json::array();
        for (auto const blocker : entry.blockers) {
            job["blockers"].push_back(blocker.value);
        }
        jobs.push_back(std::move(job));
    }
    for (auto const& [name, id] : gates_) {
        auto owners = nlohmann::json::array();
        std::uint64_t exclusive{};
        for (auto const& entry : entries_) {
            if (!entry.granted) {
                continue;
            }
            for (auto const& claim : entry.claims) {
                if (claim.name == name) {
                    owners.push_back(entry.lease.value);
                    if (claim.mode == LeaseMode::exclusive) {
                        exclusive = entry.lease.value;
                    }
                }
            }
        }
        gates.push_back(
            {{"name", name}, {"id", id.value}, {"owners", owners}, {"exclusive_owner", exclusive}});
    }
    return {{"type", "status"}, {"jobs", jobs}, {"clients", clients}, {"gates", gates}};
}
}
