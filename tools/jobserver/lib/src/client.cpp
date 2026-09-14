#include "jobserver/client.hpp"

#include "jobserver/authority.hpp"
#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cwchar>
#include <iterator>
#include <thread>

namespace jobserver {
namespace {
using Json = nlohmann::json;

auto control_timeout() -> std::chrono::milliseconds {
    constexpr auto default_timeout{std::chrono::seconds{5}};
    wchar_t value[32]{};
    auto const length{GetEnvironmentVariableW(
        L"NUKETHEBEES_JOBSERVER_TEST_IO_TIMEOUT_MS", value, std::size(value))};
    if (length == 0 || length >= std::size(value)) {
        return default_timeout;
    }
    wchar_t* end{};
    auto const parsed{std::wcstoul(value, &end, 10)};
    return end != value && *end == L'\0' && parsed != 0 ? std::chrono::milliseconds{parsed}
                                                        : default_timeout;
}

auto widen(std::string const& text) -> std::wstring {
    if (text.empty()) {
        return {};
    }
    auto const count{MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0)};
    if (count <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        text.data(),
                        static_cast<int>(text.size()),
                        result.data(),
                        count);
    return result;
}

auto quote_argument(std::wstring const& argument) -> std::wstring {
    if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring::npos) {
        return argument;
    }
    std::wstring result{L"\""};
    std::size_t backslashes{};
    for (auto const character : argument) {
        if (character == L'\\') {
            ++backslashes;
        } else if (character == L'\"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(character);
            backslashes = 0;
        } else {
            result.append(backslashes, L'\\');
            backslashes = 0;
            result.push_back(character);
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}

auto run_in_inherited_job(Command const& command) -> std::expected<int, Error> {
    std::wstring command_line{quote_argument(command.executable.wstring())};
    for (auto const& argument : command.arguments) {
        command_line.push_back(L' ');
        command_line += quote_argument(widen(argument));
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(STARTUPINFOW);
    PROCESS_INFORMATION process{};
    auto const working_directory{
        command.working_directory.empty() ? nullptr : command.working_directory.c_str()};
    if (!CreateProcessW(command.executable.c_str(),
                        command_line.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        0,
                        nullptr,
                        working_directory,
                        &startup,
                        &process)) {
        return std::unexpected(
            Error{"process_creation_failed", "Could not create the nested jobserver command"});
    }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code{};
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    return static_cast<int>(exit_code);
}

void close_handle(void*& handle) {
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(handle));
    }
    handle = nullptr;
}

auto request_daemon_start() -> bool {
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
        return false;
    }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, 5000);
    DWORD exit_code{};
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    return exit_code == 0;
}

auto connect_pipe() -> std::expected<void*, Error> {
    DWORD last_error{ERROR_SUCCESS};
    auto const test_endpoint{
        GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_TEST_PIPE", nullptr, 0) != 0};
    auto const fast_test_connect{
        GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_TEST_FAST_CONNECT", nullptr, 0) != 0};
    auto const maximum_attempts{fast_test_connect ? 2 : 50};
    for (auto attempt{0}; attempt != maximum_attempts; ++attempt) {
        auto const handle{CreateFileW(transport::pipe_name().c_str(),
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
            hello["client_version"] = "0.1.0";
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
        if (attempt == 0 && error == ERROR_FILE_NOT_FOUND && !test_endpoint) {
            static_cast<void>(request_daemon_start());
        } else if (error != ERROR_PIPE_BUSY && error != ERROR_FILE_NOT_FOUND) {
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

auto claims_json(std::vector<ResourceClaim> const& claims) -> Json {
    auto result = Json::array();
    for (auto const& claim : claims) {
        result.push_back(
            {{"name", claim.name}, {"mode", to_string(claim.mode)}, {"units", claim.units}});
    }
    return result;
}

auto metadata_json(JobMetadata const& metadata) -> Json {
    return Json{{"name", metadata.name},
                {"kind", metadata.kind},
                {"worktree", path_to_utf8(metadata.worktree)}};
}
}

Lease::Lease(void* const handle, std::string id)
    : handle_{handle}
    , id_{std::move(id)} {}
Lease::Lease(Lease&& other) noexcept
    : handle_{other.handle_}
    , id_{std::move(other.id_)} {
    other.handle_ = nullptr;
}
auto Lease::operator=(Lease&& other) noexcept -> Lease& {
    if (this != &other) {
        close_handle(handle_);
        handle_ = other.handle_;
        id_ = std::move(other.id_);
        other.handle_ = nullptr;
    }
    return *this;
}
Lease::~Lease() {
    close_handle(handle_);
}
auto Lease::id() const -> std::string const& {
    return id_;
}
auto Lease::release() -> std::expected<void, Error> {
    if (handle_ == nullptr) {
        return {};
    }
    auto const result{
        transport::write_message(handle_, Json{{"type", "release"}, {"id", id_}}.dump())};
    close_handle(handle_);
    return result;
}

auto Client::acquire(AcquireRequest const& request) -> std::expected<Lease, Error> {
    auto handle{connect_pipe()};
    if (!handle) {
        return std::unexpected(handle.error());
    }
    auto const message = Json{{"type", "acquire"},
                              {"metadata", metadata_json(request.metadata)},
                              {"resources", claims_json(request.resources)}};
    if (auto sent{transport::write_message(*handle, message.dump())}; !sent) {
        close_handle(*handle);
        return std::unexpected(sent.error());
    }
    for (;;) {
        auto response{transport::read_message(*handle)};
        if (!response) {
            close_handle(*handle);
            return std::unexpected(response.error());
        }
        auto const parsed = Json::parse(*response, nullptr, false);
        if (parsed.is_object() && parsed.value("type", "") == "queued") {
            continue;
        }
        if (!parsed.is_object() || parsed.value("type", "") != "granted") {
            close_handle(*handle);
            auto const message{parsed.is_object()
                                   ? parsed.value("message", "Invalid acquire response")
                                   : "Invalid acquire response"};
            return std::unexpected(Error{"acquire_failed", message});
        }
        return Lease{*handle, parsed.value("id", "")};
    }
}

auto Client::run(SubmitRequest const& request, OutputCallback output) -> std::expected<int, Error> {
    if (GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_JOB", nullptr, 0) != 0) {
        return run_in_inherited_job(request.command);
    }

    auto handle{connect_pipe()};
    if (!handle) {
        return std::unexpected(handle.error());
    }
    auto message = Json{
        {"type", "submit"},
        {"metadata", metadata_json(request.metadata)},
        {"resources", claims_json(request.resources)},
        {"command",
         Json{{"executable", path_to_utf8(request.command.executable)},
              {"arguments", request.command.arguments},
              {"working_directory", path_to_utf8(request.command.working_directory)},
              {"environment", Json::array()}}},
        {"disconnect_policy",
         request.disconnect_policy == DisconnectPolicy::cancel ? "cancel" : "continue"},
    };
    for (auto const& change : request.command.environment) {
        message["command"]["environment"].push_back(
            {{"name", change.name}, {"value", change.value}});
    }
    if (request.timeout) {
        message["timeout_ms"] = request.timeout->count();
    }
    if (request.suspect_after) {
        message["suspect_after_ms"] = request.suspect_after->count();
    }
    if (auto sent{transport::write_message(*handle, message.dump())}; !sent) {
        close_handle(*handle);
        return std::unexpected(sent.error());
    }
    for (;;) {
        auto response{transport::read_message(*handle)};
        if (!response) {
            close_handle(*handle);
            return std::unexpected(response.error());
        }
        auto const parsed = Json::parse(*response, nullptr, false);
        if (!parsed.is_object()) {
            close_handle(*handle);
            return std::unexpected(Error{"invalid_json", "Daemon returned invalid JSON"});
        }
        auto const type{parsed.value("type", "")};
        if (type == "output") {
            auto const decoded{protocol::decode_base64(parsed.value("data", ""))};
            if (!decoded) {
                close_handle(*handle);
                return std::unexpected(decoded.error());
            }
            output(parsed.value("stream", "stdout"), *decoded);
        } else if (type == "completed") {
            auto const exit_code{parsed.value("exit_code", 1)};
            close_handle(*handle);
            return exit_code;
        } else if (type == "error") {
            auto const error{Error{parsed.value("code", "server_error"),
                                   parsed.value("message", "Scheduler error")}};
            close_handle(*handle);
            return std::unexpected(error);
        }
    }
}

auto Client::status(bool const include_history) -> std::expected<std::string, Error> {
    auto handle{connect_pipe()};
    if (!handle) {
        return std::unexpected(handle.error());
    }
    auto const sent{transport::write_message(
        *handle, Json{{"type", "status"}, {"history", include_history}}.dump(), control_timeout())};
    if (!sent) {
        close_handle(*handle);
        return std::unexpected(sent.error());
    }
    auto response{transport::read_message(*handle, control_timeout())};
    close_handle(*handle);
    return response;
}

auto Client::ping() -> std::expected<void, Error> {
    auto handle{connect_pipe()};
    if (!handle) {
        return std::unexpected(handle.error());
    }
    auto sent{transport::write_message(*handle, Json{{"type", "ping"}}.dump(), control_timeout())};
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
    if (!parsed.is_object() || parsed.value("type", "") != "pong") {
        return std::unexpected(Error{"invalid_ping", "Daemon returned an invalid ping response"});
    }
    return {};
}

auto Client::cancel(std::string const& id, bool const kill) -> std::expected<void, Error> {
    auto handle{connect_pipe()};
    if (!handle) {
        return std::unexpected(handle.error());
    }
    auto const sent{transport::write_message(
        *handle, Json{{"type", kill ? "kill" : "cancel"}, {"id", id}}.dump(), control_timeout())};
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
    if (!parsed.is_object()) {
        return std::unexpected(Error{"invalid_json", "Daemon returned a non-object response"});
    }
    if (parsed.value("type", "") == "error") {
        return std::unexpected(Error{parsed.value("code", "server_error"),
                                     parsed.value("message", "Scheduler error")});
    }
    return {};
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

auto Client::check_daemon_recovery() -> std::expected<RecoveryAssessment, Error> {
    return check_recovery_authority([] { return Client::ping().has_value(); });
}

auto Client::force_recover_daemon() -> std::expected<void, Error> {
    return force_recover_authority([] { return Client::ping().has_value(); });
}
}
