#include "jobserver/client.hpp"

#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <thread>

namespace jobserver {
namespace {
using Json = nlohmann::json;
auto control_timeout() -> std::chrono::milliseconds {
    return std::chrono::seconds{5};
}
void close_handle(void*& handle) {
    if (handle && handle != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(handle));
    }
    handle = nullptr;
}
auto request_daemon_start() -> bool {
    auto const& sid{transport::user_sid()};
    if (sid.empty()) {
        return false;
    }
    auto const mutex_name{L"Global\\NukeTheBees.Jobserver.Start." + sid};
    auto const mutex{CreateMutexW(nullptr, FALSE, mutex_name.c_str())};
    if (mutex == nullptr) {
        return false;
    }
    auto const wait{WaitForSingleObject(mutex, 10'000)};
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) {
        CloseHandle(mutex);
        return false;
    }
    auto finish = [mutex](bool const result) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return result;
    };
    if (WaitNamedPipeW(transport::pipe_name().c_str(), 0)) {
        return finish(true);
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(STARTUPINFOW);
    PROCESS_INFORMATION process{};
    std::wstring command{L"schtasks.exe /Run /TN NukeTheBeesJobserver"};
    if (!CreateProcessW(nullptr,
                        command.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        CREATE_NO_WINDOW,
                        nullptr,
                        nullptr,
                        &startup,
                        &process)) {
        return finish(false);
    }
    CloseHandle(process.hThread);
    auto const task_wait{WaitForSingleObject(process.hProcess, 5000)};
    DWORD exit_code{};
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    if (task_wait != WAIT_OBJECT_0 || exit_code != 0) {
        return finish(false);
    }

    auto const deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
    while (std::chrono::steady_clock::now() < deadline) {
        if (WaitNamedPipeW(transport::pipe_name().c_str(), 100)) {
            return finish(true);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    return finish(false);
}

auto connect_pipe() -> std::expected<void*, Error> {
    DWORD last_error{ERROR_SUCCESS};
    auto const maximum_attempts{50};
    for (auto attempt{0}; attempt != maximum_attempts; ++attempt) {
        auto const endpoint{transport::pipe_name() + L".control"};
        auto handle{CreateFileW(endpoint.c_str(),
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_FLAG_OVERLAPPED,
                                nullptr)};
        if (handle != INVALID_HANDLE_VALUE) {
            auto hello = Json::object();
            hello["type"] = "hello";
            hello["protocol"] = Json::object();
            hello["protocol"]["major"] = protocol::major_version;
            hello["protocol"]["minor"] = protocol::minor_version;
            auto const hello_text{hello.dump()};
            if (auto sent{transport::write_message(handle, hello_text, control_timeout())}; !sent) {
                CloseHandle(handle);
                return std::unexpected(sent.error());
            }
            auto response{transport::read_message(handle, control_timeout())};
            if (!response) {
                CloseHandle(handle);
                return std::unexpected(response.error());
            }
            auto const parsed = Json::parse(*response, nullptr, false);
            if (!parsed.is_object() || parsed.value("type", "") != "hello_ack") {
                auto const message{!parsed.is_object()
                                       ? "Invalid handshake response"
                                       : parsed.value("message", "Protocol mismatch")};
                CloseHandle(handle);
                return std::unexpected(Error{"handshake_failed", message});
            }
            return handle;
        }
        auto const error{GetLastError()};
        last_error = error;
        if (error != ERROR_PIPE_BUSY) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
    return std::unexpected(Error{
        "daemon_unavailable",
        "The jobserver daemon is unavailable (Windows error " + std::to_string(last_error) +
            "); run 'jobserver start'",
    });
}

auto control_request(Json const& request, std::string_view const expected_type)
    -> std::expected<std::string, Error> {
    auto handle{connect_pipe()};
    if (!handle) {
        return std::unexpected(handle.error());
    }
    auto const sent{transport::write_message(*handle, request.dump(), control_timeout())};
    if (!sent) {
        close_handle(*handle);
        return std::unexpected(sent.error());
    }
    auto response{transport::read_message(*handle, control_timeout())};
    close_handle(*handle);
    if (!response) {
        return std::unexpected(response.error());
    }
    auto const parsed = Json::parse(*response, nullptr, false);
    if (!parsed.is_object() || parsed.value("type", "") != expected_type) {
        return std::unexpected(Error{
            parsed.is_object() ? parsed.value("code", "invalid_response") : "invalid_response",
            parsed.is_object() ? parsed.value("message", "Invalid jobserver response")
                               : "Invalid jobserver response"});
    }
    return response;
}

}
auto Client::start_daemon() -> std::expected<void, Error> {
    if (!request_daemon_start()) {
        return std::unexpected(
            Error{"start_failed", "The NukeTheBeesJobserver scheduled task is not installed"});
    }
    return {};
}

auto Client::shutdown() -> std::expected<void, Error> {
    auto handle{connect_pipe()};
    if (!handle) {
        return std::unexpected(handle.error());
    }
    auto const sent{
        transport::write_message(*handle, Json{{"type", "shutdown"}}.dump(), control_timeout())};
    if (!sent) {
        close_handle(*handle);
        return std::unexpected(sent.error());
    }
    auto response{transport::read_message(*handle, control_timeout())};
    close_handle(*handle);
    if (!response) {
        return std::unexpected(response.error());
    }
    auto const parsed = Json::parse(*response, nullptr, false);
    if (!parsed.is_object() || parsed.value("type", "") != "accepted") {
        auto const message{parsed.is_object() ? parsed.value("message", "Shutdown refused")
                                              : "Invalid shutdown response"};
        return std::unexpected(Error{"shutdown_refused", message});
    }
    return {};
}

auto Client::status() -> std::expected<std::string, Error> {
    return control_request(Json{{"type", "status"}}, "status");
}
auto Client::trace() -> std::expected<std::string, Error> {
    return control_request(Json{{"type", "trace"}}, "trace");
}
auto Client::ping() -> std::expected<void, Error> {
    auto result{control_request(Json{{"type", "ping"}}, "pong")};
    if (!result) {
        return std::unexpected(result.error());
    }
    return {};
}

}
