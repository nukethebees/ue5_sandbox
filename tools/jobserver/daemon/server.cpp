#include "server.hpp"

#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"

#include <Windows.h>

#include <sddl.h>

#include <functional>
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
auto canonical_client(HANDLE pipe, std::filesystem::path const& expected) -> DWORD {
    ULONG pid{};
    if (!GetNamedPipeClientProcessId(pipe, &pid)) {
        return 0;
    }
    auto const process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)};
    if (!process) {
        return 0;
    }
    std::wstring path(32768, L'\0');
    DWORD size{static_cast<DWORD>(path.size())};
    auto const queried{QueryFullProcessImageNameW(process, 0, path.data(), &size) != FALSE};
    CloseHandle(process);
    path.resize(size);
    std::error_code error;
    auto const matches{queried && std::filesystem::equivalent(path, expected, error)};
    return matches && !error ? pid : 0;
}
}
using server_detail::Json;

Server::Server(std::wstring endpoint, std::filesystem::path codex)
    : endpoint_{std::move(endpoint)}
    , codex_{std::move(codex)} {}
auto Server::run() -> int {
    PSECURITY_DESCRIPTOR descriptor{};
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;GA;;;WD)S:(ML;;NW;;;LW)", SDDL_REVISION_1, &descriptor, nullptr)) {
        return 1;
    }
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    std::jthread control_thread;
    std::function<int(bool)> accept;
    accept = [&](bool control) {
        bool first{true};
        while (!stopping_) {
            auto const endpoint{endpoint_ + (control ? L".control" : L"")};
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
    LocalFree(descriptor);
    return result;
}

void Server::serve_client(void* const pipe, bool const control) {
    Connection connection{.pipe = pipe};
    std::stop_callback shutdown{stop_.get_token(), [&] {
                                    connection.stop.request_stop();
                                    changed_.notify_all();
                                }};
    std::jthread writer;
    try {
        auto hello{transport::read_message(
            pipe, std::chrono::seconds{5}, std::chrono::seconds{5}, connection.stop.get_token())};
        if (!hello || !server_detail::same_user_client(static_cast<HANDLE>(pipe))) {
            return;
        }
        auto reject = [&](std::string const& code, std::string const& message) {
            static_cast<void>(transport::write_message(
                pipe,
                Json{{"type", "error"}, {"code", code}, {"message", message}}.dump(),
                std::chrono::seconds{5}));
        };
        auto const message = Json::parse(*hello);
        if (message.at("type") != "hello" ||
            message.at("protocol").at("major") != protocol::major_version) {
            reject("protocol_mismatch", "Install matching Codex and jobserver components");
            return;
        }
        if (!control) {
            auto const pid{server_detail::canonical_client(static_cast<HANDLE>(pipe), codex_)};
            if (!pid) {
                reject("client_rejected",
                       "Scheduling requires the canonical installed modified Codex process");
                return;
            }
            std::scoped_lock lock{mutex_};
            if (!processes_.insert(pid).second) {
                reject("duplicate_client", "This Codex process already has a scheduler connection");
                return;
            }
            connection.process = pid;
            connection.id = queue_.connect();
        }
        connection.replies.push_back(
            {{"type", "hello_ack"},
             {"client", connection.id.value},
             {"protocol",
              {{"major", protocol::major_version}, {"minor", protocol::minor_version}}}});
        writer = std::jthread{[&] { respond(connection); }};
        for (;;) {
            auto text{transport::read_message(
                pipe, std::nullopt, std::chrono::seconds{5}, connection.stop.get_token())};
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
        std::scoped_lock lock{mutex_};
        journal_.append("protocol_error", connection.id, error.what());
    }
    {
        std::scoped_lock lock{mutex_};
        if (connection.id.value) {
            queue_.disconnect(connection.id);
            processes_.erase(connection.process);
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
        changed_.wait(lock, [&] {
            auto const* ticket{queue_.find(connection.id)};
            return connection.closed || connection.stop.stop_requested() ||
                   !connection.replies.empty() ||
                   (ticket && ticket->granted && !connection.granted);
        });
        if (connection.closed || connection.stop.stop_requested()) {
            break;
        }
        Json reply;
        if (!connection.replies.empty()) {
            reply = std::move(connection.replies.front());
            connection.replies.pop_front();
        } else {
            connection.granted = true;
            reply = {{"type", "granted"}};
        }
        lock.unlock();
        if (!transport::write_message(connection.pipe,
                                      reply.dump(),
                                      std::chrono::seconds{5},
                                      connection.stop.get_token())) {
            connection.stop.request_stop();
        }
        lock.lock();
    }
}
void Server::request(Connection& connection, Json const& message, bool const control) {
    auto const type{message.at("type").get<std::string>()};
    auto error = [&](std::string const& code, std::string const& reason) {
        connection.replies.push_back({{"type", "error"}, {"code", code}, {"message", reason}});
    };
    if (connection.replies.size() >= 16) {
        throw std::runtime_error{"Too many unconsumed replies"};
    }
    if (!control && type == "request") {
        if (draining_) {
            error("daemon_stopping", "The daemon is shutting down");
            return;
        }
        auto const mode{message.at("mode").get<std::string>()};
        if (mode != "shared" && mode != "exclusive") {
            error("invalid_mode", "Choose shared or exclusive");
            return;
        }
        auto const result{queue_.request(connection.id,
                                         mode == "shared" ? Mode::shared : Mode::exclusive,
                                         message.at("name").get<std::string>())};
        if (!result) {
            error(result.error().code, result.error().message);
            return;
        }
        connection.granted = queue_.find(connection.id)->granted;
        connection.replies.push_back({{"type", connection.granted ? "granted" : "queued"}});
    } else if (!control && type == "release") {
        if (!queue_.release(connection.id)) {
            error("no_ticket", "This connection has no ticket");
        } else {
            connection.granted = false;
            connection.replies.push_back({{"type", "released"}});
        }
    } else if (control && type == "status") {
        auto status = queue_.status();
        status["daemon"] = {{"process_id", GetCurrentProcessId()},
                            {"protocol_major", protocol::major_version},
                            {"protocol_minor", protocol::minor_version},
                            {"version", "0.3.0"}};
        connection.replies.push_back(std::move(status));
    } else if (control && type == "trace") {
        connection.replies.push_back({{"type", "trace"}, {"events", journal_.trace()}});
    } else if (control && type == "ping") {
        connection.replies.push_back({{"type", "pong"}});
    } else if (control && type == "shutdown") {
        if (!queue_.empty()) {
            error("daemon_busy", "Queued or granted tickets must finish before shutdown");
        } else {
            connection.replies.push_back({{"type", "accepted"}});
            draining_ = true;
            connection.shutdown = true;
        }
    } else {
        error("unknown_message", "Request is not permitted on this endpoint");
    }
}
}
