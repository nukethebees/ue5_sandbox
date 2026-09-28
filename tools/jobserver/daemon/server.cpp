#include "server.hpp"

#include "jobserver/authority.hpp"
#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"

#include <Windows.h>

#include <sddl.h>

#include <iostream>

namespace jobserver {
namespace server_detail {
using Json = nlohmann::json;
auto same_user_client(HANDLE pipe) -> bool {
    if (!ImpersonateNamedPipeClient(pipe)) {
        return false;
    }
    HANDLE token{};
    std::wstring sid;
    if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token)) {
        DWORD size{};
        GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        std::vector<std::byte> storage(size);
        if (size && GetTokenInformation(token, TokenUser, storage.data(), size, &size)) {
            LPWSTR text{};
            if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(storage.data())->User.Sid,
                                       &text)) {
                sid = text;
                LocalFree(text);
            }
        }
        CloseHandle(token);
    }
    RevertToSelf();
    return !sid.empty() && sid == transport::user_sid();
}
auto heartbeat_interval() -> std::chrono::milliseconds {
    wchar_t value[32]{};
    auto const size{GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_TEST_HEARTBEAT_MS", value, 32)};
    return size && size < 32 ? std::chrono::milliseconds{std::max(1, _wtoi(value))}
                             : std::chrono::seconds{5};
}
auto positive_id(Json const& message, char const* field) -> std::uint64_t {
    auto const& value{message.at(field)};
    if (!value.is_number_unsigned() || value.get<std::uint64_t>() == 0) {
        throw std::runtime_error{"Expected positive handle"};
    }
    return value.get<std::uint64_t>();
}
}
using server_detail::Json;

Server::Server(std::filesystem::path const& directory)
    : journal_{directory}
    , queue_{journal_} {}
auto Server::run() -> int {
    PSECURITY_DESCRIPTOR descriptor{};
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;GA;;;WD)S:(ML;;NW;;;LW)", SDDL_REVISION_1, &descriptor, nullptr)) {
        return 1;
    }
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    bool authority{};
    std::jthread control_thread;
    std::function<int(bool)> accept;
    accept = [&](bool control) {
        bool first{true};
        while (!stopping_) {
            auto const endpoint{transport::pipe_name() + (control ? L".control" : L"")};
            auto pipe{CreateNamedPipeW(endpoint.c_str(),
                                       PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                                           (first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0U),
                                       PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT |
                                           PIPE_REJECT_REMOTE_CLIENTS,
                                       PIPE_UNLIMITED_INSTANCES,
                                       64U * 1024U,
                                       64U * 1024U,
                                       0,
                                       &security)};
            if (pipe == INVALID_HANDLE_VALUE) {
                std::cerr << "Cannot create scheduler pipe: " << GetLastError() << '\n';
                stopping_ = true;
                return first ? 2 : 1;
            }
            if (first && !control) {
                authority = publish_authority().has_value();
                journal_.append(
                    {.kind = EventKind::daemon_started, .value = GetCurrentProcessId()});
                journal_.append({.kind = EventKind::server_health}, "healthy");
                control_thread = std::jthread{[&] { static_cast<void>(accept(true)); }};
            }
            first = false;
            OVERLAPPED overlapped{};
            overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            bool connected{};
            if (overlapped.hEvent) {
                connected = ConnectNamedPipe(pipe, &overlapped) != FALSE;
                if (!connected) {
                    auto const error{GetLastError()};
                    if (error == ERROR_IO_PENDING) {
                        while (WaitForSingleObject(overlapped.hEvent, 50) == WAIT_TIMEOUT &&
                               !stopping_) {}
                        if (stopping_) {
                            CancelIoEx(pipe, &overlapped);
                            WaitForSingleObject(overlapped.hEvent, INFINITE);
                        } else {
                            DWORD count{};
                            connected =
                                GetOverlappedResult(pipe, &overlapped, &count, FALSE) != FALSE;
                        }
                    } else {
                        connected = error == ERROR_PIPE_CONNECTED;
                    }
                }
                CloseHandle(overlapped.hEvent);
            }
            if (!connected) {
                CloseHandle(pipe);
                continue;
            }
            {
                std::scoped_lock lock{mutex_};
                auto& count{control ? control_handlers_ : handlers_};
                if (count >= (control ? 8U : 64U)) {
                    CloseHandle(pipe);
                    continue;
                }
                ++count;
            }
            std::thread{[this, pipe, control] {
                serve_client(pipe, control);
                CloseHandle(pipe);
                {
                    std::scoped_lock lock{mutex_};
                    --(control ? control_handlers_ : handlers_);
                }
                changed_.notify_all();
            }}.detach();
        }
        return 0;
    };
    auto const result{accept(false)};
    stopping_ = true;
    stop_.request_stop();
    changed_.notify_all();
    if (control_thread.joinable()) {
        control_thread.join();
    }
    {
        std::unique_lock lock{mutex_};
        changed_.wait(lock, [&] { return handlers_ == 0 && control_handlers_ == 0; });
    }
    if (authority) {
        journal_.append({.kind = EventKind::daemon_stopped, .value = result});
        clear_authority();
    }
    LocalFree(descriptor);
    return result;
}
void Server::serve_client(void* const pipe, bool const control) {
    Connection connection{.pipe = pipe};
    std::stop_callback shutdown{stop_.get_token(), [&] { connection.stop.request_stop(); }};
    std::jthread writer;
    try {
        auto hello{transport::read_message(
            pipe, std::chrono::seconds{5}, std::chrono::seconds{5}, connection.stop.get_token())};
        if (!hello || !server_detail::same_user_client(static_cast<HANDLE>(pipe))) {
            return;
        }
        auto const message = Json::parse(*hello);
        auto const& version{message.at("protocol")};
        if (message.at("type") != "hello" || !version.at("major").is_number_unsigned() ||
            version.at("major").get<std::uint64_t>() != protocol::major_version ||
            !version.at("minor").is_number_unsigned()) {
            static_cast<void>(transport::write_message(
                pipe,
                Json{{"type", "error"},
                     {"code", "protocol_mismatch"},
                     {"message", "Install matching client and daemon protocol versions"}}
                    .dump(),
                std::chrono::seconds{5}));
            return;
        }
        {
            std::scoped_lock lock{mutex_};
            connection.id = queue_.connect();
            connection.replies.push_back(
                {{"type", "hello_ack"},
                 {"client", connection.id.value},
                 {"protocol",
                  {{"major", protocol::major_version}, {"minor", protocol::minor_version}}}});
        }
        writer = std::jthread{[&] { respond(connection); }};
        for (;;) {
            auto text{transport::read_message(
                pipe,
                control ? std::optional{std::chrono::milliseconds{5000}} : std::nullopt,
                std::chrono::seconds{5},
                connection.stop.get_token())};
            if (!text) {
                break;
            }
            auto const request_json = Json::parse(*text);
            {
                std::scoped_lock lock{mutex_};
                request(connection, request_json, control);
            }
            changed_.notify_all();
        }
    } catch (std::exception const& error) {
        journal_.append({.kind = EventKind::protocol_error, .client = connection.id}, error.what());
        // Closing an invalid session also cancels every lease associated with it.
    }
    {
        std::scoped_lock lock{mutex_};
        if (connection.id.value) {
            queue_.disconnect(connection.id);
        }
        connection.closed = true;
        if (connection.shutdown) {
            stopping_ = true;
        }
    }
    connection.stop.request_stop();
    changed_.notify_all();
    if (writer.joinable()) {
        writer.join();
    }
}
void Server::respond(Connection& connection) {
    std::unique_lock lock{mutex_};
    while (!connection.closed && !connection.stop.stop_requested()) {
        auto const changed = [&] {
            auto const* entry{queue_.find(connection.id)};
            return connection.closed || connection.stop.stop_requested() ||
                   !connection.replies.empty() ||
                   (entry &&
                    (entry->lease != connection.announced || entry->granted != connection.granted));
        };
        changed_.wait_for(lock, server_detail::heartbeat_interval(), changed);
        if (connection.closed || connection.stop.stop_requested()) {
            break;
        }
        Json reply;
        if (!connection.replies.empty()) {
            reply = std::move(connection.replies.front());
            connection.replies.pop_front();
        } else if (auto const* entry{queue_.find(connection.id)};
                   entry &&
                   (entry->lease != connection.announced || entry->granted != connection.granted)) {
            connection.announced = entry->lease;
            connection.granted = entry->granted;
            reply = {{"type", entry->granted ? "granted" : "queued"},
                     {"client", entry->client.value},
                     {"command", entry->command.value},
                     {"lease", entry->lease.value}};
        } else {
            reply = {{"type", "heartbeat"}};
        }
        lock.unlock();
        auto const sent{transport::write_message(
            connection.pipe, reply.dump(), std::chrono::seconds{5}, connection.stop.get_token())};
        if (!sent) {
            connection.stop.request_stop();
        }
        lock.lock();
    }
}
void Server::request(Connection& connection, Json const& message, bool const control) {
    auto const type{message.at("type").get<std::string>()};
    auto error = [&](std::string code, std::string reason) {
        journal_.append({.kind = EventKind::protocol_error, .client = connection.id}, reason);
        connection.replies.push_back({{"type", "error"}, {"code", code}, {"message", reason}});
    };
    if (connection.replies.size() >= 16) {
        throw std::runtime_error{"Too many unconsumed replies"};
    }
    if (type == "acquire" && !control) {
        if (draining_) {
            error("daemon_stopping", "The daemon is shutting down");
            return;
        }
        std::vector<GateClaim> gates;
        if (!message.at("gates").is_array()) {
            throw std::runtime_error{"gates must be an array"};
        }
        for (auto const& item : message.at("gates")) {
            auto const mode{item.at("mode").get<std::string>()};
            if (mode != "shared" && mode != "exclusive") {
                throw std::runtime_error{"Invalid lease mode"};
            }
            gates.push_back({item.at("name").get<std::string>(),
                             mode == "exclusive" ? LeaseMode::exclusive : LeaseMode::shared});
        }
        auto metadata = message.at("metadata");
        if (!metadata.is_object() || metadata.dump().size() > 8U * 1024U) {
            throw std::runtime_error{"Metadata must be a bounded object"};
        }
        for (auto const& item : metadata.items()) {
            if (!item.value().is_string()) {
                throw std::runtime_error{"Metadata values must be opaque strings"};
            }
        }
        auto result{queue_.acquire(connection.id, std::move(gates), std::move(metadata))};
        if (!result) {
            error(result.error().code, result.error().message);
        }
    } else if (type == "cancel") {
        auto const* entry{queue_.find(connection.id)};
        auto const lease{entry ? entry->lease : LeaseId{}};
        if (entry && queue_.cancel(connection.id, lease)) {
            connection.announced = {};
            connection.replies.push_back({{"type", "cancelled"}, {"lease", lease.value}});
        } else {
            error("not_pending", "The session has no cancellable command");
        }
    } else if (type == "release") {
        auto const lease{LeaseId{server_detail::positive_id(message, "lease")}};
        auto const& exit_code{message.at("exit_code")};
        if (!exit_code.is_number_integer() || exit_code.get<std::int64_t>() < INT_MIN ||
            exit_code.get<std::int64_t>() > INT_MAX) {
            throw std::runtime_error{"Invalid exit code"};
        }
        if (!queue_.release(connection.id, lease, exit_code.get<int>())) {
            error("invalid_lease", "Session does not own that granted lease");
        } else {
            connection.announced = {};
            connection.replies.push_back({{"type", "released"}, {"lease", lease.value}});
        }
    } else if (type == "started") {
        auto const pid{server_detail::positive_id(message, "pid")};
        if (pid > MAXDWORD || !queue_.started(connection.id,
                                              LeaseId{server_detail::positive_id(message, "lease")},
                                              static_cast<std::uint32_t>(pid))) {
            error("invalid_lease", "Cannot start an ungranted command");
        } else {
            connection.replies.push_back({{"type", "started"}});
        }
    } else if (type == "health") {
        journal_.append({.kind = EventKind::server_health, .client = connection.id},
                        message.at("state").get<std::string>());
    } else if (type == "status") {
        auto status = queue_.status();
        status["daemon"] = {{"process_id", GetCurrentProcessId()},
                            {"protocol_major", protocol::major_version},
                            {"protocol_minor", protocol::minor_version},
                            {"version", "0.2.0"}};
        connection.replies.push_back(std::move(status));
    } else if (type == "trace") {
        TraceFilter filter;
        if (message.contains("client")) {
            filter.client = ClientId{server_detail::positive_id(message, "client")};
        }
        if (message.contains("command")) {
            filter.command = CommandId{server_detail::positive_id(message, "command")};
        }
        if (message.contains("lease")) {
            filter.lease = LeaseId{server_detail::positive_id(message, "lease")};
        }
        if (message.contains("gate")) {
            auto const gate{server_detail::positive_id(message, "gate")};
            if (gate > UINT32_MAX) {
                throw std::runtime_error{"Invalid gate handle"};
            }
            filter.gate = GateId{static_cast<std::uint32_t>(gate)};
        }
        if (message.contains("limit")) {
            filter.limit = static_cast<std::size_t>(server_detail::positive_id(message, "limit"));
        }
        if (message.contains("event")) {
            for (unsigned i{}; i <= static_cast<unsigned>(EventKind::protocol_error); ++i) {
                if (event_name(static_cast<EventKind>(i)) ==
                    message.at("event").get<std::string>()) {
                    filter.kind = static_cast<EventKind>(i);
                }
            }
            if (!filter.kind) {
                throw std::runtime_error{"Unknown event kind"};
            }
        }
        connection.replies.push_back({{"type", "trace"}, {"events", journal_.trace(filter)}});
    } else if (type == "ping") {
        connection.replies.push_back({{"type", "pong"}});
    } else if (type == "shutdown") {
        if (!queue_.empty()) {
            error("daemon_busy", "Active or queued leases must drain before shutdown");
        } else {
            connection.replies.push_back({{"type", "accepted"}});
            draining_ = true;
            connection.shutdown = true;
        }
    } else {
        error("unknown_message", "Unknown request or admission on the control endpoint");
    }
}
}
