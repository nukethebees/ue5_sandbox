#include "jobserver/client.hpp"

#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"

#include <Windows.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
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

    if (auto const size{
            GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_TEST_START_MARKER", nullptr, 0)};
        size != 0) {
        std::wstring marker(size, L'\0');
        if (GetEnvironmentVariableW(
                L"NUKETHEBEES_JOBSERVER_TEST_START_MARKER", marker.data(), size) == size - 1) {
            marker.resize(size - 1);
            std::ofstream output{std::filesystem::path{marker}, std::ios::app};
            output << GetCurrentProcessId() << '\n';
        }
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

auto connect_pipe(bool const control = false, ClientId* client = nullptr)
    -> std::expected<void*, Error> {
    DWORD last_error{ERROR_SUCCESS};
    auto const test_endpoint{
        GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_TEST_PIPE", nullptr, 0) != 0};
    auto const fast_test_connect{
        GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_TEST_FAST_CONNECT", nullptr, 0) != 0};
    auto const maximum_attempts{fast_test_connect ? 2 : 50};
    for (auto attempt{0}; attempt != maximum_attempts; ++attempt) {
        auto const endpoint{transport::pipe_name() + (control ? L".control" : L"")};
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
            hello["client_version"] = "0.2.0";
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
            if (client) {
                *client = ClientId{parsed.at("client").get<std::uint64_t>()};
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

auto control_request(Json const& request, std::string_view const expected_type)
    -> std::expected<std::string, Error> {
    auto handle{connect_pipe(true)};
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
    auto handle{connect_pipe(true)};
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

auto Client::status() -> std::expected<std::string, Error> {
    return control_request(Json{{"type", "status"}}, "status");
}
auto Client::trace(Json filters) -> std::expected<std::string, Error> {
    filters["type"] = "trace";
    return control_request(filters, "trace");
}
auto Client::ping() -> std::expected<void, Error> {
    auto result{control_request(Json{{"type", "ping"}}, "pong")};
    if (!result) {
        return std::unexpected(result.error());
    }
    return {};
}

struct Session::State {
    void* pipe{};
    ClientId id{};
    HANDLE lost{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    mutable std::mutex mutex;
    std::mutex write_mutex;
    std::condition_variable changed;
    std::deque<Json> replies;
    std::optional<Error> error;
    std::jthread reader;
    ~State() {
        reader.request_stop();
        if (reader.joinable()) {
            reader.join();
        }
        close_handle(pipe);
        if (lost) {
            CloseHandle(lost);
        }
    }
    void fail(Error reason) {
        {
            std::scoped_lock lock{mutex};
            if (!error) {
                error = std::move(reason);
            }
        }
        SetEvent(lost);
        changed.notify_all();
    }
};
Session::Session(std::unique_ptr<State> state)
    : state_{std::move(state)} {}
Session::~Session() = default;
auto Session::connect() -> std::expected<std::unique_ptr<Session>, Error> {
    auto state{std::make_unique<State>()};
    if (!state->lost) {
        return std::unexpected(Error{"event_failed", "Cannot create session liveness event"});
    }
    auto pipe{connect_pipe(false, &state->id)};
    if (!pipe) {
        return std::unexpected(pipe.error());
    }
    state->pipe = *pipe;
    state->reader = std::jthread{[s = state.get()](std::stop_token stop) {
        wchar_t value[32]{};
        auto const size{
            GetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_TEST_WATCHDOG_MS", value, 32)};
        auto const watchdog{size && size < 32 ? std::chrono::milliseconds{std::max(1, _wtoi(value))}
                                              : std::chrono::seconds{20}};
        while (!stop.stop_requested()) {
            auto text{transport::read_message(s->pipe, watchdog, std::chrono::seconds{5}, stop)};
            if (!text) {
                auto error{text.error()};
                if (error.code == "read_timeout") {
                    error = {"server_unresponsive",
                             "No jobserver traffic within watchdog deadline; session closed"};
                }
                s->fail(std::move(error));
                std::scoped_lock write_lock{s->write_mutex};
                if (!stop.stop_requested()) {
                    static_cast<void>(transport::write_message(
                        s->pipe,
                        Json{{"type", "health"}, {"state", "connection_lost"}}.dump(),
                        std::chrono::milliseconds{100}));
                }
                close_handle(s->pipe);
                return;
            }
            try {
                auto message = Json::parse(*text);
                if (message.at("type") == "heartbeat") {
                    continue;
                }
                {
                    std::scoped_lock lock{s->mutex};
                    if (s->replies.size() >= 16) {
                        throw std::runtime_error{"Too many unsolicited replies"};
                    }
                    s->replies.push_back(std::move(message));
                }
                s->changed.notify_all();
            } catch (std::exception const& error) {
                s->fail({"invalid_response", error.what()});
                std::scoped_lock write_lock{s->write_mutex};
                close_handle(s->pipe);
                return;
            }
        }
    }};
    return std::unique_ptr<Session>{new Session{std::move(state)}};
}
auto Session::id() const -> ClientId {
    return state_->id;
}
auto Session::lost_event() const -> void* {
    return state_->lost;
}
auto Session::failure() const -> Error {
    std::scoped_lock lock{state_->mutex};
    return state_->error.value_or(Error{"disconnected", "Jobserver connection lost"});
}
auto Session::send(Json const& message) -> std::expected<void, Error> {
    std::scoped_lock lock{state_->write_mutex};
    {
        std::scoped_lock state_lock{state_->mutex};
        if (state_->error) {
            return std::unexpected(*state_->error);
        }
    }
    auto result{transport::write_message(state_->pipe, message.dump(), control_timeout())};
    if (!result) {
        state_->fail(result.error());
        state_->reader.request_stop();
    }
    return result;
}
auto Session::receive() -> std::expected<Json, Error> {
    std::unique_lock lock{state_->mutex};
    state_->changed.wait(lock, [&] { return state_->error || !state_->replies.empty(); });
    if (state_->error) {
        return std::unexpected(*state_->error);
    }
    auto message = std::move(state_->replies.front());
    state_->replies.pop_front();
    if (message.value("type", "") == "error") {
        return std::unexpected(Error{message.value("code", "server_error"),
                                     message.value("message", "Server rejected request")});
    }
    return message;
}
auto Session::acquire(std::vector<GateClaim> const& gates,
                      Json const& metadata,
                      std::stop_token const stop,
                      std::function<void(Json const&)> const& state)
    -> std::expected<Grant, Error> {
    auto claims = Json::array();
    for (auto const& gate : gates) {
        claims.push_back({{"name", gate.name},
                          {"mode", gate.mode == LeaseMode::exclusive ? "exclusive" : "shared"}});
    }
    if (auto sent{send(Json{{"type", "acquire"}, {"gates", claims}, {"metadata", metadata}})};
        !sent) {
        return std::unexpected(sent.error());
    }
    std::atomic<bool> cancelling{};
    std::optional<std::stop_callback<std::function<void()>>> cancel{
        std::in_place, stop, [&] {
            cancelling = true;
            static_cast<void>(send(Json{{"type", "cancel"}}));
        }};
    for (;;) {
        auto response{receive()};
        if (!response) {
            return std::unexpected(response.error());
        }
        auto const type{response->value("type", "")};
        if (state) {
            state(*response);
        }
        if (type == "cancelled") {
            return std::unexpected(Error{"cancelled", "Pending command cleared without launching"});
        }
        if (type == "granted") {
            cancel.reset();
            if (!cancelling) {
                return Grant{state_->id,
                             CommandId{response->at("command").get<std::uint64_t>()},
                             LeaseId{response->at("lease").get<std::uint64_t>()}};
            }
        }
        if (type != "queued" && type != "granted") {
            return std::unexpected(Error{"invalid_response", "Expected admission response"});
        }
    }
}
auto Session::started(Grant const& grant, std::uint32_t const pid) -> std::expected<void, Error> {
    if (auto sent{send(Json{{"type", "started"}, {"lease", grant.lease.value}, {"pid", pid}})};
        !sent) {
        return sent;
    }
    auto response{receive()};
    if (!response) {
        return std::unexpected(response.error());
    }
    if (response->value("type", "") != "started") {
        return std::unexpected(Error{"invalid_response", "Expected start acknowledgement"});
    }
    return {};
}
auto Session::release(Grant const& grant, int const exit_code) -> std::expected<void, Error> {
    if (auto sent{send(
            Json{{"type", "release"}, {"lease", grant.lease.value}, {"exit_code", exit_code}})};
        !sent) {
        return sent;
    }
    auto response{receive()};
    if (!response) {
        return std::unexpected(response.error());
    }
    if (response->value("type", "") != "released") {
        return std::unexpected(Error{"invalid_response", "Expected release acknowledgement"});
    }
    return {};
}
}
