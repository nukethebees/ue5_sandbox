#include "server.hpp"

#include "jobserver/authority.hpp"
#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"
#include "test_barrier.hpp"

#include <Windows.h>

#include <sddl.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <thread>

namespace jobserver {
namespace {
using Json = nlohmann::json;
inline constexpr std::size_t maximum_client_handlers{64};
inline constexpr std::size_t maximum_control_handlers{8};

auto client_io_timeout() -> std::chrono::milliseconds {
    static auto const timeout = [] -> std::chrono::milliseconds {
        constexpr auto default_timeout{std::chrono::seconds{30}};
        wchar_t value[32]{};
        auto const length{GetEnvironmentVariableW(
            L"NUKETHEBEES_JOBSERVER_TEST_IO_TIMEOUT_MS", value, std::size(value))};
        if (length == 0 || length >= std::size(value)) {
            return default_timeout;
        }
        wchar_t* end{};
        auto const parsed{std::wcstoul(value, &end, 10)};
        if (end == value || *end != L'\0' || parsed == 0 ||
            parsed > static_cast<unsigned long>(std::numeric_limits<std::int64_t>::max())) {
            return default_timeout;
        }
        return std::chrono::milliseconds{parsed};
    }();
    return timeout;
}

auto initial_message_timeout() -> std::chrono::milliseconds {
    static auto const timeout = [] -> std::chrono::milliseconds {
        constexpr auto default_timeout{std::chrono::seconds{5}};
        wchar_t value[32]{};
        auto const length{GetEnvironmentVariableW(
            L"NUKETHEBEES_JOBSERVER_TEST_IO_TIMEOUT_MS", value, std::size(value))};
        if (length == 0 || length >= std::size(value)) {
            return default_timeout;
        }
        wchar_t* end{};
        auto const parsed{std::wcstoul(value, &end, 10)};
        if (end == value || *end != L'\0' || parsed == 0 ||
            parsed > static_cast<unsigned long>(std::numeric_limits<std::int64_t>::max())) {
            return default_timeout;
        }
        return std::chrono::milliseconds{parsed};
    }();
    return timeout;
}

auto maximum_starting_time() -> std::chrono::milliseconds {
    constexpr auto default_timeout{std::chrono::seconds{30}};
    wchar_t value[32]{};
    auto const length{GetEnvironmentVariableW(
        L"NUKETHEBEES_JOBSERVER_TEST_STARTING_TIMEOUT_MS", value, std::size(value))};
    if (length == 0 || length >= std::size(value)) {
        return default_timeout;
    }
    wchar_t* end{};
    auto const parsed{std::wcstoul(value, &end, 10)};
    return end != value && *end == L'\0' && parsed != 0 ? std::chrono::milliseconds{parsed}
                                                        : default_timeout;
}

auto queue_heartbeat_interval() -> std::chrono::milliseconds {
    constexpr auto default_interval{std::chrono::seconds{5}};
    wchar_t value[32]{};
    auto const length{GetEnvironmentVariableW(
        L"NUKETHEBEES_JOBSERVER_TEST_HEARTBEAT_MS", value, std::size(value))};
    if (length == 0 || length >= std::size(value)) {
        return default_interval;
    }
    wchar_t* end{};
    auto const parsed{std::wcstoul(value, &end, 10)};
    return end != value && *end == L'\0' && parsed != 0 ? std::chrono::milliseconds{parsed}
                                                        : default_interval;
}

class JobCompletionGuard {
  public:
    explicit JobCompletionGuard(std::function<void()> finish)
        : finish_{std::move(finish)} {}
    ~JobCompletionGuard() { finish_(); }

    JobCompletionGuard(JobCompletionGuard const&) = delete;
    auto operator=(JobCompletionGuard const&) -> JobCompletionGuard& = delete;
  private:
    std::function<void()> finish_;
};

auto valid_text(Json const& value, bool const allow_empty = true) -> bool {
    return value.is_string() && (allow_empty || !value.get_ref<std::string const&>().empty()) &&
           value.get_ref<std::string const&>().find('\0') == std::string::npos;
}

auto valid_integer(Json const& value, std::uint64_t const minimum, std::uint64_t const maximum)
    -> bool {
    if (!value.is_number_integer() ||
        (!value.is_number_unsigned() && value.get<std::int64_t>() < 0)) {
        return false;
    }
    auto const number{value.get<std::uint64_t>()};
    return number >= minimum && number <= maximum;
}

auto parse_claims(Json const& json) -> std::expected<std::vector<ResourceClaim>, Error> {
    std::vector<ResourceClaim> result;
    if (!json.is_array()) {
        return std::unexpected(Error{"invalid_resources", "resources must be an array"});
    }
    for (auto const& item : json) {
        if (!item.is_object() || !item.contains("name") || !valid_text(item["name"], false) ||
            !item.contains("mode") || !valid_text(item["mode"], false) ||
            (item.contains("units") &&
             !valid_integer(item["units"], 1, std::numeric_limits<std::uint32_t>::max()))) {
            return std::unexpected(
                Error{"invalid_resource", "Invalid resource field types or unit range"});
        }
        auto const mode{claim_mode_from_string(item.value("mode", ""))};
        auto const name{item.value("name", "")};
        auto const units{item.value("units", 1U)};
        if (!mode || name.empty() || units == 0) {
            return std::unexpected(Error{
                "invalid_resource", "Every resource requires a name, mode, and positive units"});
        }
        result.push_back(ResourceClaim{.name = name, .mode = *mode, .units = units});
    }
    return result;
}

auto validate_request(Json const& json) -> std::expected<void, Error> {
    auto invalid = [](std::string field) -> std::expected<void, Error> {
        return std::unexpected(Error{"invalid_request", "Invalid or missing field: " + field});
    };
    if (!json.contains("type") || !valid_text(json["type"], false)) {
        return invalid("type");
    }
    auto const type{json["type"].get<std::string>()};
    if (type == "status" && json.contains("history") && !json["history"].is_boolean()) {
        return invalid("history");
    }
    if (type == "cancel" || type == "kill" || type == "validate_nested") {
        auto const field{type == "validate_nested" ? "parent_id" : "id"};
        if (!json.contains(field) || !valid_text(json[field], false)) {
            return invalid(field);
        }
    }
    if (type != "acquire" && type != "submit" && type != "validate_nested") {
        return {};
    }
    if (json.contains("resources")) {
        if (auto claims{parse_claims(json["resources"])}; !claims) {
            return std::unexpected(claims.error());
        }
    }
    if (type == "validate_nested") {
        return {};
    }
    if (json.contains("metadata")) {
        auto const& metadata{json["metadata"]};
        if (!metadata.is_object()) {
            return invalid("metadata");
        }
        for (auto const field : {"name", "kind", "worktree"}) {
            if (metadata.contains(field) && !valid_text(metadata[field])) {
                return invalid(std::string{"metadata."} + field);
            }
        }
    }
    if (type != "submit") {
        return {};
    }
    if (json.contains("disconnect_policy") &&
        (!valid_text(json["disconnect_policy"]) ||
         (json["disconnect_policy"] != "cancel" && json["disconnect_policy"] != "continue"))) {
        return invalid("disconnect_policy");
    }
    auto const maximum_duration{std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::duration::max())
                                    .count()};
    for (auto const field : {"timeout_ms", "suspect_after_ms"}) {
        if (json.contains(field) &&
            !valid_integer(json[field], 1, static_cast<std::uint64_t>(maximum_duration))) {
            return invalid(field);
        }
    }
    if (!json.contains("command") || !json["command"].is_object()) {
        return invalid("command");
    }
    auto const& command{json["command"]};
    if (!command.contains("executable") || !valid_text(command["executable"], false)) {
        return invalid("command.executable");
    }
    if (command.contains("working_directory") && !valid_text(command["working_directory"])) {
        return invalid("command.working_directory");
    }
    if (command.contains("arguments")) {
        auto const& arguments{command["arguments"]};
        if (!arguments.is_array() ||
            !std::ranges::all_of(arguments, [](auto const& value) { return valid_text(value); })) {
            return invalid("command.arguments");
        }
    }
    if (command.contains("environment")) {
        auto const& environment{command["environment"]};
        if (!environment.is_array()) {
            return std::unexpected(Error{"invalid_environment", "Environment must be an array"});
        }
        for (auto const& change : environment) {
            if (!change.is_object() || !change.contains("name") ||
                !valid_text(change["name"], false) ||
                change["name"].get_ref<std::string const&>().find('=') != std::string::npos ||
                !change.contains("value") ||
                (!change["value"].is_null() && !valid_text(change["value"]))) {
                return std::unexpected(Error{"invalid_environment", "Invalid environment change"});
            }
        }
    }
    return {};
}

auto parse_metadata(Json const& json) -> JobMetadata {
    return JobMetadata{
        .name = json.value("name", "unnamed"),
        .kind = json.value("kind", "command"),
        .worktree = path_from_utf8(json.value("worktree", "")),
    };
}

auto pipe_connected(HANDLE const pipe) -> bool {
    DWORD available{};
    return PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) != FALSE;
}

auto token_user(HANDLE const token) -> std::vector<std::byte> {
    DWORD size{};
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<std::byte> storage(size);
    if (size == 0 || !GetTokenInformation(token, TokenUser, storage.data(), size, &size)) {
        return {};
    }
    return storage;
}

auto same_user_client(HANDLE const pipe) -> bool {
    if (!ImpersonateNamedPipeClient(pipe)) {
        return false;
    }

    HANDLE client_token{};
    auto const opened_client{OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &client_token)};
    auto const client_storage{opened_client ? token_user(client_token) : std::vector<std::byte>{}};
    if (client_token != nullptr) {
        CloseHandle(client_token);
    }
    RevertToSelf();

    HANDLE daemon_token{};
    if (client_storage.empty() ||
        !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &daemon_token)) {
        return false;
    }
    auto const daemon_storage{token_user(daemon_token)};
    CloseHandle(daemon_token);
    if (daemon_storage.empty()) {
        return false;
    }

    auto const client_user{reinterpret_cast<TOKEN_USER const*>(client_storage.data())};
    auto const daemon_user{reinterpret_cast<TOKEN_USER const*>(daemon_storage.data())};
    return EqualSid(client_user->User.Sid, daemon_user->User.Sid) != FALSE;
}

auto make_pipe_security()
    -> std::expected<std::pair<SECURITY_ATTRIBUTES, PSECURITY_DESCRIPTOR>, Error> {
    constexpr auto descriptor{L"D:P(A;;GA;;;WD)S:(ML;;NW;;;LW)"};
    PSECURITY_DESCRIPTOR security_descriptor{};
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            descriptor, SDDL_REVISION_1, &security_descriptor, nullptr)) {
        return std::unexpected(Error{"security_initialization_failed",
                                     "Could not construct the named-pipe security descriptor"});
    }
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    attributes.lpSecurityDescriptor = security_descriptor;
    return std::pair{attributes, security_descriptor};
}
}

auto Server::write_client(void* const pipe, std::string const& message)
    -> std::expected<void, Error> {
    return transport::write_message(
        pipe, message, initial_message_timeout(), connection_stop_.get_token());
}

void Server::send_error(void* const pipe, Error const& error) {
    static_cast<void>(write_client(
        pipe, Json{{"type", "error"}, {"code", error.code}, {"message", error.message}}.dump()));
}

auto Server::run() -> int {
    started_at_ = std::chrono::system_clock::now();
    started_steady_ = std::chrono::steady_clock::now();
    last_audit_ms_.store(
        std::chrono::duration_cast<std::chrono::milliseconds>(started_at_.time_since_epoch())
            .count());
    load_history();
    auto pipe_security{make_pipe_security()};
    if (!pipe_security) {
        std::cerr << pipe_security.error().message << '\n';
        return 1;
    }
    auto& [security, descriptor]{*pipe_security};
    std::jthread audit_thread{[this](std::stop_token const stop_token) { audit_loop(stop_token); }};
    auto authority_published{false};
    std::atomic<int> control_result{};
    std::jthread control_thread;
    auto finish = [&](int const result) {
        stopping_.store(true);
        connection_stop_.request_stop();
        if (control_thread.joinable()) {
            control_thread.join();
        }
        LocalFree(descriptor);
        std::unique_lock lock{handlers_mutex_};
        handlers_finished_.wait(lock, [&] { return active_handlers_ == 0; });
        if (authority_published) {
            clear_authority();
        }
        return result == 0 ? control_result.load() : result;
    };
    std::function<int(bool)> accept_clients;
    accept_clients = [&](bool const control) -> int {
        bool first{true};
        for (;;) {
            if (stopping_.load()) {
                return 0;
            }
            auto const flags{PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                             (first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0U)};
            auto const endpoint{transport::pipe_name() + (control ? L".control" : L"")};
            auto const pipe{CreateNamedPipeW(endpoint.c_str(),
                                             flags,
                                             PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT |
                                                 PIPE_REJECT_REMOTE_CLIENTS,
                                             PIPE_UNLIMITED_INSTANCES,
                                             64U * 1024U,
                                             64U * 1024U,
                                             0,
                                             &security)};
            if (pipe == INVALID_HANDLE_VALUE) {
                auto const error{GetLastError()};
                std::cerr << (first ? "Another jobserver daemon is already running\n"
                                    : "Could not create scheduler pipe (Windows error " +
                                          std::to_string(error) + ")\n");
                stopping_.store(true);
                return first ? 2 : 1;
            }
            if (first && !control) {
                if (!history_path_.empty()) {
                    try {
                        log_store_ =
                            std::make_unique<LogStore>(history_path_.parent_path() / "logs");
                    } catch (...) {
                        std::cerr << "Job log storage is unavailable\n";
                    }
                }
                auto const published{publish_authority()};
                if (!published) {
                    std::cerr << published.error().message
                              << "; forced recovery will be unavailable\n";
                } else {
                    authority_published = true;
                }
                test_barrier("after_authority_publication");
                control_thread = std::jthread{[&] { control_result.store(accept_clients(true)); }};
            }
            first = false;
            OVERLAPPED overlapped{};
            overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            auto connected{false};
            if (overlapped.hEvent != nullptr) {
                connected = ConnectNamedPipe(pipe, &overlapped) != FALSE;
                if (!connected) {
                    auto const error{GetLastError()};
                    if (error == ERROR_IO_PENDING) {
                        while (WaitForSingleObject(overlapped.hEvent, 50) == WAIT_TIMEOUT &&
                               !stopping_.load()) {}
                        if (stopping_.load()) {
                            CancelIoEx(pipe, &overlapped);
                            WaitForSingleObject(overlapped.hEvent, INFINITE);
                        } else {
                            DWORD transferred{};
                            connected = GetOverlappedResult(
                                            pipe, &overlapped, &transferred, FALSE) != FALSE;
                        }
                    } else {
                        connected = error == ERROR_PIPE_CONNECTED;
                    }
                }
                CloseHandle(overlapped.hEvent);
            }
            if (!connected) {
                CloseHandle(pipe);
                if (stopping_.load()) {
                    return 0;
                }
                continue;
            }
            auto admitted{false};
            {
                std::scoped_lock const lock{handlers_mutex_};
                auto& lane_handlers{control ? active_control_handlers_ : active_job_handlers_};
                auto const capacity{control ? maximum_control_handlers : maximum_client_handlers};
                if (lane_handlers < capacity) {
                    ++lane_handlers;
                    ++active_handlers_;
                    admitted = true;
                }
            }
            if (!admitted) {
                ++rejected_clients_;
                DisconnectNamedPipe(pipe);
                CloseHandle(pipe);
                continue;
            }
            std::thread{[this, pipe, control] {
                try {
                    serve_client(pipe, control);
                } catch (std::exception const& error) {
                    std::cerr << "Client handler failed while processing a protocol message: "
                              << error.what() << '\n';
                    DisconnectNamedPipe(pipe);
                    CloseHandle(pipe);
                } catch (...) {
                    std::cerr << "Client handler failed while processing a protocol message\n";
                    DisconnectNamedPipe(pipe);
                    CloseHandle(pipe);
                }
                {
                    std::scoped_lock const lock{handlers_mutex_};
                    --active_handlers_;
                    --(control ? active_control_handlers_ : active_job_handlers_);
                }
                handlers_finished_.notify_all();
            }}.detach();
        }
    };
    return finish(accept_clients(false));
}

void Server::serve_client(void* const native_pipe, bool const control) {
    auto const pipe{static_cast<HANDLE>(native_pipe)};
    auto hello{transport::read_message(
        pipe, initial_message_timeout(), initial_message_timeout(), connection_stop_.get_token())};
    if (!hello) {
        CloseHandle(pipe);
        return;
    }
    if (!same_user_client(pipe)) {
        send_error(pipe, Error{"access_denied", "The client belongs to a different user"});
        CloseHandle(pipe);
        return;
    }
    auto const hello_json = Json::parse(*hello, nullptr, false);
    auto compatible{hello_json.is_object()};
    compatible = compatible && hello_json.contains("type") && hello_json["type"].is_string() &&
                 hello_json["type"].get<std::string>() == "hello";
    compatible =
        compatible && hello_json.contains("protocol") && hello_json["protocol"].is_object();
    compatible = compatible && hello_json["protocol"].contains("major") &&
                 valid_integer(hello_json["protocol"]["major"],
                               0,
                               std::numeric_limits<std::uint32_t>::max()) &&
                 hello_json["protocol"]["major"].get<std::uint64_t>() == protocol::major_version;
    compatible = compatible && (!hello_json["protocol"].contains("minor") ||
                                valid_integer(hello_json["protocol"]["minor"],
                                              0,
                                              std::numeric_limits<std::uint32_t>::max()));
    if (!compatible) {
        send_error(pipe,
                   Error{"protocol_mismatch", "Client and daemon protocol major versions differ"});
        CloseHandle(pipe);
        return;
    }
    auto acknowledgement = Json::object();
    acknowledgement["type"] = "hello_ack";
    acknowledgement["protocol"] = Json::object();
    acknowledgement["protocol"]["major"] = protocol::major_version;
    acknowledgement["protocol"]["minor"] = protocol::minor_version;
    acknowledgement["server_version"] = "0.1.0";
    if (!write_client(pipe, acknowledgement.dump())) {
        CloseHandle(pipe);
        return;
    }
    auto request{transport::read_message(
        pipe, initial_message_timeout(), initial_message_timeout(), connection_stop_.get_token())};
    if (!request) {
        CloseHandle(pipe);
        return;
    }
    auto const json = Json::parse(*request, nullptr, false);
    auto const shutdown_response{json.is_object() && json.contains("type") &&
                                 json["type"] == "shutdown"};
    if (!json.is_object()) {
        send_error(pipe, Error{"invalid_json", "Request is not valid JSON"});
    } else if (auto valid{validate_request(json)}; !valid) {
        send_error(pipe, valid.error());
    } else {
        auto const type{json.contains("type") && json["type"].is_string()
                            ? json["type"].get<std::string>()
                            : std::string{}};
        if (control && (type == "acquire" || type == "submit")) {
            send_error(pipe, Error{"control_only", "Submit jobs through the job endpoint"});
        } else if (type == "acquire") {
            handle_acquire(pipe, *request);
        } else if (type == "submit") {
            handle_submit(pipe, *request);
        } else if (type == "validate_nested") {
            handle_validate_nested(pipe, *request);
        } else if (type == "status") {
            handle_status(pipe, json.value("history", false));
        } else if (type == "ping") {
            static_cast<void>(write_client(pipe, Json{{"type", "pong"}}.dump()));
        } else if (type == "cancel") {
            handle_cancel(pipe, *request, false);
        } else if (type == "kill") {
            handle_cancel(pipe, *request, true);
        } else if (type == "shutdown") {
            handle_shutdown(pipe);
        } else {
            send_error(pipe, Error{"unknown_message", "Unknown request type"});
        }
    }
    // Wait for EOF promptly, preserving unread replies but allowing idle shutdown to interrupt.
    static_cast<void>(transport::read_message(pipe,
                                              initial_message_timeout(),
                                              initial_message_timeout(),
                                              shutdown_response ? std::stop_token{}
                                                                : connection_stop_.get_token()));
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

void Server::handle_acquire(void* const pipe, std::string const& message) {
    auto const json = Json::parse(message);
    auto claims{parse_claims(json.value("resources", Json::array()))};
    if (!claims) {
        send_error(pipe, claims.error());
        return;
    }
    if (auto valid{scheduler_.validate_claims(*claims)}; !valid) {
        send_error(pipe, valid.error());
        return;
    }
    std::string id;
    {
        std::scoped_lock const lock{admission_mutex_};
        if (!accepting_jobs_) {
            send_error(pipe, Error{"daemon_stopping", "The daemon is shutting down"});
            return;
        }
        id = scheduler_.enqueue(parse_metadata(json.value("metadata", Json::object())),
                                std::move(*claims));
    }
    JobCompletionGuard const completion_guard{[this, &id] { finish_job(id); }};
    test_barrier("after_admission");
    auto next_heartbeat{std::chrono::steady_clock::now() + queue_heartbeat_interval()};
    while (!scheduler_.try_grant(id)) {
        test_barrier("after_queue_poll");
        auto const state{scheduler_.state(id)};
        if (state == JobState::starting) {
            break;
        }
        if (!state || *state != JobState::queued) {
            send_error(pipe, Error{"job_cancelled", "The queued lease request was cancelled"});
            return;
        }
        if (!pipe_connected(static_cast<HANDLE>(pipe))) {
            static_cast<void>(scheduler_.cancel_queued(id));
            return;
        }
        if (std::chrono::steady_clock::now() >= next_heartbeat) {
            if (!write_client(pipe, Json{{"type", "queued"}, {"id", id}}.dump())) {
                static_cast<void>(scheduler_.cancel_queued(id));
                return;
            }
            next_heartbeat = std::chrono::steady_clock::now() + queue_heartbeat_interval();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
    test_barrier("after_resource_grant");
    if (scheduler_.state(id) != JobState::starting) {
        send_error(pipe, Error{"start_expired", "Lease startup exceeded the daemon deadline"});
        return;
    }
    {
        std::scoped_lock const lock{leases_mutex_};
        if (scheduler_.state(id) != JobState::starting) {
            send_error(pipe, Error{"start_expired", "Lease startup exceeded the daemon deadline"});
            return;
        }
        leases_.insert(id);
        scheduler_.set_state(id, JobState::running);
    }
    if (!write_client(pipe, Json{{"type", "granted"}, {"id", id}}.dump())) {
        scheduler_.release(id, JobState::interrupted);
        return;
    }
    auto const release{transport::read_message(
        pipe, std::nullopt, initial_message_timeout(), connection_stop_.get_token())};
    auto const release_json = release ? Json::parse(*release, nullptr, false) : Json();
    auto const valid_release{release_json.is_object() && release_json.contains("type") &&
                             release_json["type"] == "release" && release_json.contains("id") &&
                             valid_text(release_json["id"], false) && release_json["id"] == id};
    scheduler_.release(id, valid_release ? JobState::succeeded : JobState::interrupted);
    {
        std::scoped_lock const lock{leases_mutex_};
        leases_.erase(id);
    }
    record_history(id);
    if (release && !valid_release) {
        send_error(
            pipe,
            Error{"invalid_request", "Lease release requires its granted ID and release type"});
    }
}

void Server::handle_validate_nested(void* const pipe, std::string const& message) {
    auto const json = Json::parse(message);
    if (!json.contains("parent_id") || !json["parent_id"].is_string()) {
        send_error(pipe, Error{"nested_parent_not_active", "Missing nested parent ID"});
        return;
    }
    auto const parent_id{json["parent_id"].get<std::string>()};
    auto claims{parse_claims(json.value("resources", Json::array()))};
    if (!claims) {
        send_error(pipe, claims.error());
        return;
    }
    if (scheduler_.state(parent_id) != JobState::running) {
        send_error(pipe, Error{"nested_parent_not_active", "Nested parent is not running"});
        return;
    }
    std::shared_ptr<Supervisor> supervisor;
    {
        std::scoped_lock const lock{supervisors_mutex_};
        auto const found{supervisors_.find(parent_id)};
        if (found != supervisors_.end()) {
            supervisor = found->second;
        }
    }
    ULONG process_id{};
    if (!supervisor || !GetNamedPipeClientProcessId(static_cast<HANDLE>(pipe), &process_id) ||
        !supervisor->contains_process(process_id)) {
        send_error(pipe,
                   Error{"nested_parent_mismatch", "Caller is not in the parent's process tree"});
        return;
    }
    if (auto valid{scheduler_.validate_nested_claims(parent_id, *claims)}; !valid) {
        send_error(pipe, valid.error());
        return;
    }
    static_cast<void>(
        write_client(pipe, Json{{"type", "nested_validated"}, {"parent_id", parent_id}}.dump()));
}

void Server::handle_submit(void* const pipe, std::string const& message) {
    auto const json = Json::parse(message);
    auto claims{parse_claims(json.value("resources", Json::array()))};
    if (!claims) {
        send_error(pipe, claims.error());
        return;
    }
    if (auto valid{scheduler_.validate_claims(*claims)}; !valid) {
        send_error(pipe, valid.error());
        return;
    }
    auto const metadata{parse_metadata(json.value("metadata", Json::object()))};
    auto const continue_on_disconnect{json.value("disconnect_policy", "cancel") == "continue"};
    std::string id;
    {
        std::scoped_lock const lock{admission_mutex_};
        if (!accepting_jobs_) {
            send_error(pipe, Error{"daemon_stopping", "The daemon is shutting down"});
            return;
        }
        id = scheduler_.enqueue(metadata, std::move(*claims));
    }
    JobCompletionGuard const completion_guard{[this, &id] { finish_job(id); }};
    test_barrier("after_admission");
    auto next_heartbeat{std::chrono::steady_clock::now() + queue_heartbeat_interval()};
    while (!scheduler_.try_grant(id)) {
        test_barrier("after_queue_poll");
        auto const state{scheduler_.state(id)};
        if (state == JobState::starting) {
            break;
        }
        if (!state || *state != JobState::queued) {
            static_cast<void>(write_client(pipe,
                                           Json{{"type", "completed"},
                                                {"id", id},
                                                {"state", to_string(JobState::killed)},
                                                {"exit_code", 130}}
                                               .dump()));
            return;
        }
        if (!pipe_connected(static_cast<HANDLE>(pipe))) {
            static_cast<void>(scheduler_.cancel_queued(id));
            return;
        }
        if (std::chrono::steady_clock::now() >= next_heartbeat) {
            if (!write_client(pipe, Json{{"type", "queued"}, {"id", id}}.dump())) {
                static_cast<void>(scheduler_.cancel_queued(id));
                return;
            }
            next_heartbeat = std::chrono::steady_clock::now() + queue_heartbeat_interval();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
    test_barrier("after_resource_grant");
    if (scheduler_.state(id) != JobState::starting) {
        send_error(pipe, Error{"start_expired", "Command startup exceeded the daemon deadline"});
        return;
    }
    auto const command_json = json.value("command", Json::object());
    Command command{
        .executable = path_from_utf8(command_json.value("executable", "")),
        .arguments = command_json.value("arguments", std::vector<std::string>{}),
        .working_directory = path_from_utf8(command_json.value("working_directory", "")),
        .environment = {},
    };
    auto const changes = command_json.value("environment", Json::array());
    if (!changes.is_array()) {
        send_error(pipe, Error{"invalid_environment", "Environment must be an array"});
        return;
    }
    for (auto const& change : changes) {
        if (!change.is_object() || !change.contains("name") || !change["name"].is_string() ||
            !change.contains("value") ||
            (!change["value"].is_string() && !change["value"].is_null())) {
            send_error(pipe, Error{"invalid_environment", "Invalid environment change"});
            return;
        }
        command.environment.push_back(
            {.name = change["name"].get<std::string>(),
             .value = change["value"].is_null()
                        ? std::nullopt
                        : std::optional{change["value"].get<std::string>()}});
    }
    command.environment.push_back({.name = "NUKETHEBEES_JOBSERVER_JOB", .value = id});
    std::optional<std::chrono::milliseconds> timeout;
    if (json.contains("timeout_ms")) {
        timeout = std::chrono::milliseconds{json["timeout_ms"].get<std::int64_t>()};
    }
    std::optional<std::chrono::milliseconds> suspect_after;
    if (json.contains("suspect_after_ms")) {
        suspect_after = std::chrono::milliseconds{json["suspect_after_ms"].get<std::int64_t>()};
    }
    std::mutex output_mutex;
    std::unique_ptr<JobLogs> logs;
    try {
        if (log_store_) {
            logs = log_store_->open(id);
        }
    } catch (...) {}
    if (!logs || !logs->available()) {
        std::cerr << "Job logs are unavailable; continuing without persistent output\n";
    }
    auto supervisor{std::make_shared<Supervisor>()};
    {
        std::scoped_lock const lock{supervisors_mutex_};
        if (scheduler_.state(id) != JobState::starting) {
            send_error(pipe,
                       Error{"start_expired", "Command startup exceeded the daemon deadline"});
            return;
        }
        supervisors_[id] = supervisor;
        scheduler_.set_state(id, JobState::running);
    }
    std::atomic client_writable{true};
    auto result{supervisor->run(
        command,
        timeout,
        suspect_after,
        [&](std::string const& stream, std::string const& text) {
            std::scoped_lock const lock{output_mutex};
            if (logs) {
                logs->write(stream, text);
            }
            if (client_writable.load() &&
                !transport::write_message(pipe,
                                          Json{{"type", "output"},
                                               {"id", id},
                                               {"stream", stream},
                                               {"data", protocol::encode_base64(text)}}
                                              .dump(),
                                          client_io_timeout(),
                                          supervisor->output_stop_token())) {
                client_writable.store(false);
                DisconnectNamedPipe(static_cast<HANDLE>(pipe));
            }
        },
        [this, &id](JobHealth const health, std::string reason) {
            scheduler_.set_health(id, health, std::move(reason));
        },
        [pipe, continue_on_disconnect, &client_writable] {
            return continue_on_disconnect ||
                   (client_writable.load() && pipe_connected(static_cast<HANDLE>(pipe)));
        })};
    if (!result) {
        test_barrier("before_resource_release");
        scheduler_.release(id, JobState::failed);
        {
            std::scoped_lock const lock{supervisors_mutex_};
            supervisors_.erase(id);
        }
        if (client_writable.load()) {
            send_error(pipe, result.error());
        }
        return;
    }
    auto final_state{JobState::failed};
    if (result->timed_out) {
        final_state = JobState::timed_out;
    } else if (result->killed) {
        final_state = JobState::killed;
    } else if (result->exit_code == 0) {
        final_state = JobState::succeeded;
    }
    test_barrier("before_resource_release");
    scheduler_.release(id, final_state);
    {
        std::scoped_lock const lock{supervisors_mutex_};
        supervisors_.erase(id);
    }
    record_history(id);
    auto const reported_exit_code{result->timed_out ? 124
                                                    : (result->termination_exit_code != 0
                                                           ? result->termination_exit_code
                                                           : result->exit_code)};
    if (client_writable.load()) {
        static_cast<void>(write_client(pipe,
                                       Json{
                                           {"type", "completed"},
                                           {"id", id},
                                           {"state", to_string(final_state)},
                                           {"exit_code", reported_exit_code},
                                       }
                                           .dump()));
    }
}

void Server::handle_status(void* const pipe, bool const include_history) {
    test_barrier("before_status_response");
    auto const snapshot{scheduler_.snapshot()};
    auto const now{std::chrono::system_clock::now()};
    auto entries = Json::array();
    for (auto const& entry : snapshot.entries) {
        if (entry.state == JobState::succeeded || entry.state == JobState::failed ||
            entry.state == JobState::timed_out || entry.state == JobState::killed ||
            entry.state == JobState::interrupted) {
            continue;
        }
        auto claims = Json::array();
        for (auto const& claim : entry.claims) {
            claims.push_back(
                {{"name", claim.name}, {"mode", to_string(claim.mode)}, {"units", claim.units}});
        }
        entries.push_back({
            {"id", entry.id},
            {"claims", std::move(claims)},
            {"name", entry.metadata.name},
            {"kind", entry.metadata.kind},
            {"worktree", path_to_utf8(entry.metadata.worktree)},
            {"state", to_string(entry.state)},
            {"health", to_string(entry.health)},
            {"health_reason", entry.health_reason},
            {"blockers", entry.blockers},
            {"queued_ms",
             std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.queued_at).count()},
            {"running_ms",
             entry.started_at == std::chrono::system_clock::time_point{}
                 ? 0
                 : std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.started_at)
                       .count()},
        });
    }
    if (include_history) {
        std::scoped_lock const lock{history_mutex_};
        for (auto const& serialized : history_) {
            auto const entry = Json::parse(serialized, nullptr, false);
            if (entry.is_object()) {
                entries.push_back(entry);
            }
        }
    }
    auto resources = Json::array();
    for (auto const& resource : snapshot.resources) {
        resources.push_back({
            {"name", resource.name},
            {"used", resource.used},
            {"capacity", resource.capacity},
            {"exclusive", resource.exclusive},
        });
    }
    auto diagnostics = Json::array();
    {
        std::scoped_lock const lock{diagnostics_mutex_};
        diagnostics = diagnostics_;
    }
    std::size_t supervised_jobs{};
    std::size_t leases{};
    std::size_t active_handlers{};
    std::size_t job_handlers{};
    std::size_t control_handlers{};
    {
        std::scoped_lock const lock{supervisors_mutex_};
        supervised_jobs = supervisors_.size();
    }
    {
        std::scoped_lock const lock{leases_mutex_};
        leases = leases_.size();
    }
    {
        std::scoped_lock const lock{handlers_mutex_};
        active_handlers = active_handlers_;
        job_handlers = active_job_handlers_;
        control_handlers = active_control_handlers_;
    }
    auto const last_audit_ms{last_audit_ms_.load()};
    auto const now_ms{std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count()};
    auto const started_ms{
        std::chrono::duration_cast<std::chrono::milliseconds>(started_at_.time_since_epoch())
            .count()};
    auto const daemon = Json{{"process_id", GetCurrentProcessId()},
                             {"started_ms", started_ms},
                             {"uptime_ms",
                              std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - started_steady_)
                                  .count()},
                             {"last_audit_ms", last_audit_ms},
                             {"last_audit_age_ms", now_ms - last_audit_ms},
                             {"active_handlers", active_handlers},
                             {"handler_capacity", maximum_client_handlers},
                             {"job_handlers", job_handlers},
                             {"control_handlers", control_handlers},
                             {"control_handler_capacity", maximum_control_handlers},
                             {"rejected_clients", rejected_clients_.load()},
                             {"supervised_jobs", supervised_jobs},
                             {"leases", leases},
                             {"scheduler_entries", snapshot.entries.size()},
                             {"version", "0.1.0"},
                             {"protocol_major", protocol::major_version},
                             {"protocol_minor", protocol::minor_version}};
    auto response = Json::object();
    response["type"] = "status";
    response["daemon"] = daemon;
    response["jobs"] = std::move(entries);
    response["resources"] = std::move(resources);
    response["diagnostics"] = std::move(diagnostics);
    response["heartbeat_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
    auto const sent{write_client(pipe, response.dump())};
    if (!sent && sent.error().code == "payload_too_large") {
        send_error(pipe, sent.error());
    }
}

void Server::audit_loop(std::stop_token const stop_token) {
    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        last_audit_ms_.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count());
        std::unordered_set<std::string> owned_jobs;
        std::vector<std::string> findings;
        {
            // Keep owner publication and the scheduler audit on the same side of this snapshot.
            std::scoped_lock const lock{supervisors_mutex_, leases_mutex_};
            for (auto const& [id, supervisor] : supervisors_) {
                static_cast<void>(supervisor);
                owned_jobs.insert(id);
            }
            owned_jobs.insert(leases_.begin(), leases_.end());
            if (!owned_jobs.empty()) {
                test_barrier("after_audit_owner_snapshot");
            }
            findings = scheduler_.audit_and_recover(owned_jobs, maximum_starting_time());
        }
        auto const snapshot{scheduler_.snapshot()};
        std::unordered_set<std::string> terminal_jobs;
        for (auto const& entry : snapshot.entries) {
            if (entry.state == JobState::succeeded || entry.state == JobState::failed ||
                entry.state == JobState::timed_out || entry.state == JobState::killed ||
                entry.state == JobState::interrupted) {
                terminal_jobs.insert(entry.id);
            }
        }
        {
            std::scoped_lock const lock{supervisors_mutex_};
            std::erase_if(supervisors_,
                          [&](auto const& item) { return terminal_jobs.contains(item.first); });
        }
        {
            std::scoped_lock const lock{leases_mutex_};
            std::erase_if(leases_,
                          [&](std::string const& id) { return terminal_jobs.contains(id); });
        }
        if (findings.empty()) {
            continue;
        }
        std::scoped_lock const lock{diagnostics_mutex_};
        for (auto& finding : findings) {
            std::cerr << "Jobserver invariant recovery: " << finding << '\n';
            diagnostics_.push_back(std::move(finding));
        }
        if (diagnostics_.size() > 100) {
            diagnostics_.erase(diagnostics_.begin(), diagnostics_.end() - 100);
        }
    }
}

void Server::load_history() {
    char* test_data{};
    std::size_t test_data_size{};
    if (_dupenv_s(&test_data, &test_data_size, "NUKETHEBEES_JOBSERVER_TEST_DATA") == 0 &&
        test_data != nullptr) {
        history_path_ = std::filesystem::path{test_data} / "history.jsonl";
        std::free(test_data);
    }

    char* local_app_data{};
    std::size_t size{};
    if (_dupenv_s(&local_app_data, &size, "LOCALAPPDATA") != 0 || local_app_data == nullptr) {
        return;
    }
    if (history_path_.empty()) {
        history_path_ = std::filesystem::path{local_app_data} / "NukeTheBees" / "jobserver" /
                        "data" / "history.jsonl";
    }
    std::free(local_app_data);
    std::ifstream input{history_path_};
    std::string line;
    while (std::getline(input, line)) {
        auto const parsed = Json::parse(line, nullptr, false);
        if (parsed.is_object()) {
            history_.push_back(std::move(line));
            if (history_.size() > 1000) {
                history_.erase(history_.begin());
            }
        }
    }
}

void Server::record_history(std::string const& id) noexcept {
    try {
        auto const found{scheduler_.take_completed(id)};
        if (!found || history_path_.empty()) {
            return;
        }
        test_barrier("during_history_recording");
        auto entry = Json::object();
        entry["id"] = found->id;
        entry["name"] = found->metadata.name;
        entry["kind"] = found->metadata.kind;
        entry["worktree"] = path_to_utf8(found->metadata.worktree);
        entry["state"] = to_string(found->state);
        entry["health"] = to_string(found->health);
        entry["health_reason"] = found->health_reason;
        entry["blockers"] = Json::array();
        entry["duration_ms"] = found->started_at == std::chrono::system_clock::time_point{}
                                 ? 0
                                 : std::chrono::duration_cast<std::chrono::milliseconds>(
                                       found->finished_at - found->started_at)
                                       .count();
        auto const serialized{entry.dump()};

        std::scoped_lock const lock{history_mutex_};
        history_.push_back(serialized);
        auto const compact{history_.size() > 1000};
        if (compact) {
            history_.erase(history_.begin(),
                           history_.begin() + static_cast<std::ptrdiff_t>(history_.size() - 1000));
        }
        std::error_code filesystem_error;
        std::filesystem::create_directories(history_path_.parent_path(), filesystem_error);
        if (filesystem_error) {
            std::cerr << "Could not create the job history directory\n";
            return;
        }
        {
            std::ofstream output{history_path_, std::ios::app};
            output << serialized << '\n';
        }
        if (compact) {
            auto replacement_path{history_path_};
            replacement_path += ".tmp";
            {
                std::ofstream replacement{replacement_path, std::ios::trunc};
                for (auto const& historical_entry : history_) {
                    replacement << historical_entry << '\n';
                }
            }
            if (!MoveFileExW(replacement_path.c_str(),
                             history_path_.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                std::error_code error;
                std::filesystem::remove(replacement_path, error);
            }
        }
    } catch (...) {
        std::cerr << "Could not persist job history\n";
    }
}

void Server::handle_cancel(void* const pipe, std::string const& message, bool const kill) {
    auto const id{Json::parse(message).value("id", "")};
    if (scheduler_.cancel_queued(id)) {
        static_cast<void>(write_client(pipe, Json{{"type", "accepted"}, {"id", id}}.dump()));
        return;
    }
    std::shared_ptr<Supervisor> supervisor;
    {
        std::scoped_lock const lock{supervisors_mutex_};
        auto const found{supervisors_.find(id)};
        if (found != supervisors_.end()) {
            supervisor = found->second;
        }
    }
    if (!supervisor) {
        send_error(pipe, Error{"job_not_running", "No queued or running job has that id"});
        return;
    }
    if (kill) {
        supervisor->kill();
    } else {
        supervisor->cancel();
    }
    static_cast<void>(write_client(pipe, Json{{"type", "accepted"}, {"id", id}}.dump()));
}

void Server::finish_job(std::string const& id) noexcept {
    try {
        static_cast<void>(scheduler_.cancel_queued(id));
        scheduler_.release(id, JobState::interrupted);
        {
            std::scoped_lock const lock{supervisors_mutex_};
            supervisors_.erase(id);
        }
        {
            std::scoped_lock const lock{leases_mutex_};
            leases_.erase(id);
        }
        record_history(id);
    } catch (...) {
        std::cerr << "Could not finalize job " << id << '\n';
    }
}

void Server::handle_shutdown(void* const pipe) {
    std::scoped_lock const admission_lock{admission_mutex_};
    auto const snapshot{scheduler_.snapshot()};
    auto const active{std::ranges::any_of(snapshot.entries, [](QueueEntry const& entry) {
        return entry.state == JobState::queued || entry.state == JobState::starting ||
               entry.state == JobState::running || entry.state == JobState::cancelling;
    })};
    if (active) {
        send_error(pipe,
                   Error{"jobs_active", "Shutdown is refused while jobs are queued or running"});
        return;
    }
    accepting_jobs_ = false;
    stopping_.store(true);
    static_cast<void>(transport::write_message(
        pipe, Json{{"type", "accepted"}}.dump(), initial_message_timeout()));
}
}
