#include "jobserver/client.hpp"
#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"
#include "log_store.hpp"

#include <Windows.h>

#include <tlhelp32.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
using Json = nlohmann::json;

struct ChildProcess {
    HANDLE process{};
    HANDLE thread{};
};

struct CommandResult {
    DWORD exit_code{};
    std::string output;
};

class TestIoTimeout {
  public:
    explicit TestIoTimeout(char const* const value) {
        char previous[32]{};
        GetEnvironmentVariableA(
            "NUKETHEBEES_JOBSERVER_TEST_IO_TIMEOUT_MS", previous, std::size(previous));
        previous_ = previous;
        static_cast<void>(_putenv_s("NUKETHEBEES_JOBSERVER_TEST_IO_TIMEOUT_MS", value));
    }
    ~TestIoTimeout() {
        static_cast<void>(_putenv_s("NUKETHEBEES_JOBSERVER_TEST_IO_TIMEOUT_MS", previous_.c_str()));
    }
  private:
    std::string previous_;
};

struct TestPipes {
    std::vector<HANDLE> handles;
    ~TestPipes() {
        for (auto const handle : handles) {
            CloseHandle(handle);
        }
    }
};

auto quote(std::wstring const& value) -> std::wstring {
    return L"\"" + value + L"\"";
}

auto launch(std::filesystem::path const& executable,
            std::wstring arguments = {},
            std::filesystem::path const& working_directory = {}) -> ChildProcess {
    auto command_line{quote(executable.wstring())};
    if (!arguments.empty()) {
        command_line += L" " + arguments;
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(STARTUPINFOW);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(),
                        command_line.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        CREATE_NO_WINDOW,
                        nullptr,
                        working_directory.empty() ? nullptr : working_directory.c_str(),
                        &startup,
                        &process)) {
        return {};
    }
    return {.process = process.hProcess, .thread = process.hThread};
}

auto run_and_capture(std::filesystem::path const& executable,
                     std::wstring arguments = {},
                     std::filesystem::path const& working_directory = {})
    -> std::optional<CommandResult> {
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    HANDLE output_read{};
    HANDLE output_write{};
    if (!CreatePipe(&output_read, &output_write, &attributes, 0) ||
        !SetHandleInformation(output_read, HANDLE_FLAG_INHERIT, 0)) {
        if (output_read != nullptr) {
            CloseHandle(output_read);
        }
        if (output_write != nullptr) {
            CloseHandle(output_write);
        }
        return std::nullopt;
    }

    auto command_line{quote(executable.wstring())};
    if (!arguments.empty()) {
        command_line += L" " + arguments;
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = output_write;
    startup.hStdError = output_write;
    PROCESS_INFORMATION process{};
    auto const started{
        CreateProcessW(executable.c_str(),
                       command_line.data(),
                       nullptr,
                       nullptr,
                       TRUE,
                       CREATE_NO_WINDOW,
                       nullptr,
                       working_directory.empty() ? nullptr : working_directory.c_str(),
                       &startup,
                       &process)};
    CloseHandle(output_write);
    if (!started) {
        CloseHandle(output_read);
        return std::nullopt;
    }
    CloseHandle(process.hThread);

    std::string output;
    for (;;) {
        std::array<char, 4096> buffer{};
        DWORD read{};
        if (ReadFile(
                output_read, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
            output.append(buffer.data(), read);
            continue;
        }
        if (GetLastError() == ERROR_BROKEN_PIPE) {
            break;
        }
        CloseHandle(output_read);
        CloseHandle(process.hProcess);
        return std::nullopt;
    }
    CloseHandle(output_read);

    if (WaitForSingleObject(process.hProcess, 5'000) != WAIT_OBJECT_0) {
        CloseHandle(process.hProcess);
        return std::nullopt;
    }
    DWORD exit_code{};
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    return CommandResult{.exit_code = exit_code, .output = std::move(output)};
}

auto connect_raw_pipe(bool const control = false) -> HANDLE {
    auto const endpoint{jobserver::transport::pipe_name() + (control ? L".control" : L"")};
    for (auto attempt{0}; attempt != 50; ++attempt) {
        auto const pipe{CreateFileW(endpoint.c_str(),
                                    GENERIC_READ | GENERIC_WRITE,
                                    0,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_FLAG_OVERLAPPED,
                                    nullptr)};
        if (pipe != INVALID_HANDLE_VALUE) {
            return pipe;
        }
        std::this_thread::sleep_for(10ms);
    }
    return INVALID_HANDLE_VALUE;
}

auto connect_sync_raw_pipe() -> HANDLE {
    for (auto attempt{0}; attempt != 100; ++attempt) {
        auto const pipe{CreateFileW(jobserver::transport::pipe_name().c_str(),
                                    GENERIC_READ | GENERIC_WRITE,
                                    0,
                                    nullptr,
                                    OPEN_EXISTING,
                                    0,
                                    nullptr)};
        if (pipe != INVALID_HANDLE_VALUE) {
            return pipe;
        }
        std::this_thread::sleep_for(1ms);
    }
    return INVALID_HANDLE_VALUE;
}

auto write_raw(HANDLE const pipe, std::span<std::byte const> const bytes) -> bool {
    DWORD written{};
    return WriteFile(pipe, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) !=
               FALSE &&
           written == bytes.size();
}

void close(ChildProcess& process) {
    if (process.thread != nullptr) {
        CloseHandle(process.thread);
        process.thread = nullptr;
    }
    if (process.process != nullptr) {
        CloseHandle(process.process);
        process.process = nullptr;
    }
}

auto wait_for_exit(ChildProcess& process, std::chrono::milliseconds const timeout)
    -> std::optional<DWORD> {
    if (process.process == nullptr ||
        WaitForSingleObject(process.process, static_cast<DWORD>(timeout.count())) !=
            WAIT_OBJECT_0) {
        return std::nullopt;
    }
    DWORD exit_code{};
    GetExitCodeProcess(process.process, &exit_code);
    return exit_code;
}

auto process_ids_named(std::wstring const& executable_name) -> std::vector<DWORD> {
    auto const snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }
    std::vector<DWORD> result;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (executable_name == entry.szExeFile) {
                result.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    std::ranges::sort(result);
    return result;
}

auto helper_children_of(DWORD const parent_id) -> std::vector<DWORD> {
    auto const snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }
    std::vector<DWORD> result;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ParentProcessID == parent_id &&
                std::wstring_view{entry.szExeFile} == L"jobserver-test-helper.exe") {
                result.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

auto observe_process(DWORD const process_id) -> ChildProcess {
    return {.process =
                OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE,
                            FALSE,
                            process_id)};
}

void expect_crash_cleanup(ChildProcess& process) {
    EXPECT_NE(process.process, nullptr);
    if (process.process != nullptr) {
        auto const exited{wait_for_exit(process, 2s)};
        EXPECT_TRUE(exited) << "Supervised process survived daemon termination";
        if (!exited) {
            EXPECT_TRUE(TerminateProcess(process.process, 99));
            EXPECT_TRUE(wait_for_exit(process, 2s));
        }
    }
    close(process);
}

class TestBarrier {
  public:
    TestBarrier(std::string const& phase, int const index) {
        auto const suffix{std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(index)};
        reached_name_ = L"Local\\NukeTheBees.Jobserver.Barrier.Reached." + suffix;
        release_name_ = L"Local\\NukeTheBees.Jobserver.Barrier.Release." + suffix;
        reached_ = CreateEventW(nullptr, TRUE, FALSE, reached_name_.c_str());
        release_ = CreateEventW(nullptr, TRUE, FALSE, release_name_.c_str());
        static_cast<void>(_putenv_s("NUKETHEBEES_JOBSERVER_TEST_BARRIER", phase.c_str()));
        static_cast<void>(
            _wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_BARRIER_REACHED", reached_name_.c_str()));
        static_cast<void>(
            _wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_BARRIER_RELEASE", release_name_.c_str()));
    }
    ~TestBarrier() {
        release();
        CloseHandle(reached_);
        CloseHandle(release_);
        static_cast<void>(_putenv_s("NUKETHEBEES_JOBSERVER_TEST_BARRIER", ""));
        static_cast<void>(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_BARRIER_REACHED", L""));
        static_cast<void>(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_BARRIER_RELEASE", L""));
    }

    TestBarrier(TestBarrier const&) = delete;
    auto operator=(TestBarrier const&) -> TestBarrier& = delete;

    [[nodiscard]] auto valid() const -> bool { return reached_ != nullptr && release_ != nullptr; }
    [[nodiscard]] auto wait(std::chrono::milliseconds const timeout) const -> bool {
        return WaitForSingleObject(reached_, static_cast<DWORD>(timeout.count())) == WAIT_OBJECT_0;
    }
    void release() const {
        if (release_ != nullptr) {
            SetEvent(release_);
        }
    }
  private:
    std::wstring reached_name_;
    std::wstring release_name_;
    HANDLE reached_{};
    HANDLE release_{};
};

auto test_request(std::string name, jobserver::ClaimMode const mode) -> jobserver::AcquireRequest {
    return {
        .metadata = {.name = std::move(name), .kind = "test", .worktree = {}},
        .resources = {{.name = "integration-resource", .mode = mode}},
    };
}

auto submit_request(std::string name,
                    std::vector<jobserver::ResourceClaim> resources,
                    std::vector<std::string> arguments,
                    std::optional<std::chrono::milliseconds> timeout = std::nullopt)
    -> jobserver::SubmitRequest {
    return {
        .metadata = {.name = std::move(name), .kind = "test", .worktree = {}},
        .command = {.executable = JOBSERVER_TEST_HELPER_PATH,
                    .arguments = std::move(arguments),
                    .working_directory =
                        std::filesystem::path{JOBSERVER_TEST_HELPER_PATH}.parent_path(),
                    .environment = {}},
        .resources = std::move(resources),
        .timeout = timeout,
        .suspect_after = std::nullopt,
        .disconnect_policy = jobserver::DisconnectPolicy::continue_job,
    };
}

auto raw_submit_message(std::string name, std::vector<std::string> arguments) -> Json {
    auto message = Json::object();
    message["type"] = "submit";
    message["metadata"] = Json{{"name", std::move(name)}, {"kind", "test"}, {"worktree", ""}};
    message["resources"] = Json::array();
    message["command"] = Json{
        {"executable", jobserver::path_to_utf8(std::filesystem::path{JOBSERVER_TEST_HELPER_PATH})},
        {"arguments", std::move(arguments)},
        {"working_directory",
         jobserver::path_to_utf8(std::filesystem::path{JOBSERVER_TEST_HELPER_PATH}.parent_path())},
        {"environment", Json::array()},
    };
    message["disconnect_policy"] = "continue";
    return message;
}

auto connect_and_handshake_raw_pipe(bool const control = false) -> HANDLE {
    auto pipe{connect_raw_pipe(control)};
    if (pipe == INVALID_HANDLE_VALUE) {
        return pipe;
    }
    auto const hello = Json{{"type", "hello"},
                            {"protocol",
                             {{"major", jobserver::protocol::major_version},
                              {"minor", jobserver::protocol::minor_version}}}};
    if (!jobserver::transport::write_message(pipe, hello.dump()) ||
        !jobserver::transport::read_message(pipe)) {
        CloseHandle(pipe);
        return INVALID_HANDLE_VALUE;
    }
    return pipe;
}

auto find_job(std::string const& name) -> std::optional<Json> {
    for (auto attempt{0}; attempt != 100; ++attempt) {
        auto status{jobserver::Client::status()};
        if (status) {
            auto const json = Json::parse(*status);
            for (auto const& job : json.value("jobs", Json::array())) {
                if (job.value("name", "") == name) {
                    return job;
                }
            }
        }
        std::this_thread::sleep_for(10ms);
    }
    return std::nullopt;
}

auto find_job_in_state(std::string const& name, std::string const& state) -> std::optional<Json> {
    for (auto attempt{0}; attempt != 100; ++attempt) {
        auto status{jobserver::Client::status()};
        if (status) {
            auto const json = Json::parse(*status);
            for (auto const& job : json.value("jobs", Json::array())) {
                if (job.value("name", "") == name && job.value("state", "") == state) {
                    return job;
                }
            }
        }
        std::this_thread::sleep_for(10ms);
    }
    return std::nullopt;
}

auto wait_for_job_to_disappear(std::string const& name, std::chrono::milliseconds const timeout)
    -> bool {
    auto const deadline{std::chrono::steady_clock::now() + timeout};
    while (std::chrono::steady_clock::now() < deadline) {
        auto status{jobserver::Client::status()};
        if (status) {
            auto const json = Json::parse(*status);
            auto const jobs = json.find("jobs");
            if (json.is_object() && jobs != json.end() && jobs->is_array() &&
                std::ranges::none_of(*jobs, [&](Json const& job) {
                    return job.is_object() && job.value("name", "") == name;
                })) {
                return true;
            }
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

auto wait_for_file(std::filesystem::path const& path, std::chrono::milliseconds const timeout)
    -> bool {
    auto const deadline{std::chrono::steady_clock::now() + timeout};
    while (std::chrono::steady_clock::now() < deadline) {
        if (std::filesystem::exists(path)) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

class JobserverIntegration : public ::testing::Test {
  protected:
    static void SetUpTestSuite() {
        auto const process_id{GetCurrentProcessId()};
        pipe_name_ = LR"(\\.\pipe\NukeTheBees.Jobserver.Tests.)" + std::to_wstring(process_id);
        data_path_ = std::filesystem::temp_directory_path() /
                     ("NukeTheBees-jobserver-tests-" + std::to_string(process_id));
        std::filesystem::remove_all(data_path_);
        std::filesystem::create_directories(data_path_);
        ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_PIPE", pipe_name_.c_str()), 0);
        ASSERT_EQ(_putenv_s("NUKETHEBEES_JOBSERVER_TEST_DATA", data_path_.string().c_str()), 0);
        ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_IO_TIMEOUT_MS", L"250"), 0);
        ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_HEARTBEAT_MS", L"100"), 0);
        ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_JOB", L""), 0);
    }

    static void TearDownTestSuite() {
        std::filesystem::remove_all(data_path_);
        static_cast<void>(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_PIPE", L""));
        static_cast<void>(_putenv_s("NUKETHEBEES_JOBSERVER_TEST_DATA", ""));
        static_cast<void>(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_IO_TIMEOUT_MS", L""));
        static_cast<void>(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_HEARTBEAT_MS", L""));
    }

    void SetUp() override { start_daemon(); }

    void TearDown() override { stop_daemon(); }

    void start_daemon() {
        daemon_ = launch(JOBSERVER_DAEMON_PATH);
        ASSERT_NE(daemon_.process, nullptr);
        close_thread();
        auto status{jobserver::Client::status()};
        ASSERT_TRUE(status.has_value()) << status.error().message;
    }

    void stop_daemon() {
        if (daemon_.process == nullptr) {
            return;
        }
        if (WaitForSingleObject(daemon_.process, 0) == WAIT_TIMEOUT) {
            auto shutdown{jobserver::Client::shutdown()};
            EXPECT_TRUE(shutdown.has_value()) << shutdown.error().message;
            if (!shutdown) {
                EXPECT_TRUE(TerminateProcess(daemon_.process, 99));
            }
        }
        auto const exit_code{wait_for_exit(daemon_, 5s)};
        EXPECT_TRUE(exit_code.has_value());
        close(daemon_);
    }

    void close_thread() {
        if (daemon_.thread != nullptr) {
            CloseHandle(daemon_.thread);
            daemon_.thread = nullptr;
        }
    }

    ChildProcess daemon_;
    inline static std::wstring pipe_name_;
    inline static std::filesystem::path data_path_;
};

TEST_F(JobserverIntegration, EnforcesSingleInstanceAndRestartsWithNoLiveOwnership) {
    auto second{launch(JOBSERVER_DAEMON_PATH)};
    ASSERT_NE(second.process, nullptr);
    auto const second_exit{wait_for_exit(second, 2s)};
    ASSERT_TRUE(second_exit.has_value());
    EXPECT_EQ(*second_exit, 2U);
    close(second);

    stop_daemon();
    start_daemon();
    auto status{jobserver::Client::status()};
    ASSERT_TRUE(status.has_value());
    auto const json = Json::parse(*status);
    EXPECT_TRUE(json.value("jobs", Json::array()).empty());
}

TEST_F(JobserverIntegration, ConcurrentDaemonStartsElectOneAuthority) {
    stop_daemon();
    std::vector<ChildProcess> candidates;
    for (auto index{0}; index != 8; ++index) {
        candidates.push_back(launch(JOBSERVER_DAEMON_PATH));
        ASSERT_NE(candidates.back().process, nullptr);
    }
    std::this_thread::sleep_for(200ms);

    auto running_count{0};
    for (auto& candidate : candidates) {
        if (WaitForSingleObject(candidate.process, 0) == WAIT_TIMEOUT) {
            ++running_count;
            daemon_ = candidate;
            candidate = {};
        } else {
            auto const exit_code{wait_for_exit(candidate, 2s)};
            ASSERT_TRUE(exit_code.has_value());
            EXPECT_EQ(*exit_code, 2U);
        }
        close(candidate);
    }
    ASSERT_EQ(running_count, 1);
    close_thread();
    EXPECT_TRUE(jobserver::Client::status().has_value());
}

TEST_F(JobserverIntegration, IncompleteClientCannotPreventDaemonShutdown) {
    auto const stalled_pipe{connect_raw_pipe()};
    ASSERT_NE(stalled_pipe, INVALID_HANDLE_VALUE);

    auto const start{std::chrono::steady_clock::now()};
    auto shutdown{jobserver::Client::shutdown()};
    ASSERT_TRUE(shutdown.has_value()) << shutdown.error().message;
    auto const exit_code{wait_for_exit(daemon_, 2s)};
    EXPECT_TRUE(exit_code.has_value());
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);

    CloseHandle(stalled_pipe);
    close(daemon_);
}

TEST_F(JobserverIntegration, PingConfirmsResponsiveControlPlane) {
    EXPECT_TRUE(jobserver::Client::ping().has_value());

    auto status{jobserver::Client::status()};
    ASSERT_TRUE(status.has_value());
    auto const status_json = Json::parse(*status);
    ASSERT_TRUE(status_json.is_object()) << status_json.dump();
    ASSERT_TRUE(status_json.contains("daemon")) << status_json.dump();
    ASSERT_TRUE(status_json["daemon"].is_object()) << status_json.dump();
    auto const daemon = status_json["daemon"];
    EXPECT_EQ(daemon.value("process_id", 0U), GetProcessId(daemon_.process));
    EXPECT_GE(daemon.value("uptime_ms", -1LL), 0);
    EXPECT_GE(daemon.value("last_audit_ms", 0LL), 1);
    EXPECT_GE(daemon.value("active_handlers", 0U), 1U);
    EXPECT_EQ(daemon.value("protocol_major", 0), jobserver::protocol::major_version);
}

TEST_F(JobserverIntegration, RecoveryRefusesResponsiveDaemon) {
    auto assessment{jobserver::Client::check_daemon_recovery()};
    ASSERT_TRUE(assessment.has_value());
    EXPECT_TRUE(assessment->responsive);
    EXPECT_TRUE(assessment->authority_valid);
    EXPECT_TRUE(assessment->process_running);
    EXPECT_FALSE(assessment->recoverable);
    EXPECT_EQ(assessment->process_id, GetProcessId(daemon_.process));

    auto recovered{jobserver::Client::force_recover_daemon()};
    ASSERT_FALSE(recovered.has_value());
    EXPECT_EQ(recovered.error().code, "daemon_healthy");
    EXPECT_EQ(WaitForSingleObject(daemon_.process, 0), WAIT_TIMEOUT);
}

TEST_F(JobserverIntegration, RecoveryTerminatesOnlyValidatedUnresponsiveDaemon) {
    stop_daemon();
    {
        TestBarrier barrier{"after_authority_publication", 2};
        ASSERT_TRUE(barrier.valid());
        daemon_ = launch(JOBSERVER_DAEMON_PATH);
        ASSERT_NE(daemon_.process, nullptr);
        close_thread();
        ASSERT_TRUE(barrier.wait(2s));

        auto assessment{jobserver::Client::check_daemon_recovery()};
        ASSERT_TRUE(assessment.has_value());
        EXPECT_FALSE(assessment->responsive);
        EXPECT_TRUE(assessment->authority_valid);
        EXPECT_TRUE(assessment->process_running);
        EXPECT_TRUE(assessment->recoverable);

        auto recovered{jobserver::Client::force_recover_daemon()};
        ASSERT_TRUE(recovered.has_value()) << recovered.error().message;
        auto const exit_code{wait_for_exit(daemon_, 2s)};
        ASSERT_TRUE(exit_code.has_value());
        EXPECT_EQ(*exit_code, 70U);
        close(daemon_);
        EXPECT_FALSE(std::filesystem::exists(data_path_ / "authority.json"));
    }
    start_daemon();
}

TEST_F(JobserverIntegration, RecoveryRefusesTamperedAuthorityRecord) {
    stop_daemon();
    {
        TestBarrier barrier{"after_authority_publication", 3};
        ASSERT_TRUE(barrier.valid());
        daemon_ = launch(JOBSERVER_DAEMON_PATH);
        ASSERT_NE(daemon_.process, nullptr);
        close_thread();
        ASSERT_TRUE(barrier.wait(2s));

        auto const authority_path{data_path_ / "authority.json"};
        Json authority;
        {
            std::ifstream input{authority_path};
            ASSERT_TRUE(input.good());
            input >> authority;
        }
        authority["creation_time"] = authority.value("creation_time", 0ULL) + 1;
        {
            std::ofstream output{authority_path, std::ios::trunc};
            output << authority.dump();
        }

        auto recovered{jobserver::Client::force_recover_daemon()};
        ASSERT_FALSE(recovered.has_value());
        EXPECT_EQ(recovered.error().code, "authority_mismatch");
        EXPECT_EQ(WaitForSingleObject(daemon_.process, 0), WAIT_TIMEOUT);

        ASSERT_TRUE(TerminateProcess(daemon_.process, 71));
        ASSERT_TRUE(wait_for_exit(daemon_, 2s).has_value());
        close(daemon_);
    }
    start_daemon();
}

TEST_F(JobserverIntegration, RecoveryCleansRecordForExitedDaemon) {
    stop_daemon();
    ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_FAST_CONNECT", L"1"), 0);
    {
        TestBarrier barrier{"after_authority_publication", 4};
        ASSERT_TRUE(barrier.valid());
        daemon_ = launch(JOBSERVER_DAEMON_PATH);
        ASSERT_NE(daemon_.process, nullptr);
        close_thread();
        ASSERT_TRUE(barrier.wait(2s));
        ASSERT_TRUE(TerminateProcess(daemon_.process, 72));
        ASSERT_TRUE(wait_for_exit(daemon_, 2s).has_value());
        close(daemon_);
        barrier.release();

        auto recovered{jobserver::Client::force_recover_daemon()};
        ASSERT_TRUE(recovered.has_value()) << recovered.error().message;
        EXPECT_FALSE(std::filesystem::exists(data_path_ / "authority.json"));
    }
    ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_FAST_CONNECT", L""), 0);
    start_daemon();
}

TEST_F(JobserverIntegration, ConcurrentRecoveryKillsValidatedDaemonOnce) {
    stop_daemon();
    ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_FAST_CONNECT", L"1"), 0);
    {
        TestBarrier barrier{"after_authority_publication", 5};
        ASSERT_TRUE(barrier.valid());
        daemon_ = launch(JOBSERVER_DAEMON_PATH);
        ASSERT_NE(daemon_.process, nullptr);
        close_thread();
        ASSERT_TRUE(barrier.wait(2s));

        auto first{launch(JOBSERVER_TEST_CLIENT_PATH, L"recover")};
        auto second{launch(JOBSERVER_TEST_CLIENT_PATH, L"recover")};
        ASSERT_NE(first.process, nullptr);
        ASSERT_NE(second.process, nullptr);
        auto const first_result{wait_for_exit(first, 3s)};
        auto const second_result{wait_for_exit(second, 3s)};
        ASSERT_TRUE(first_result.has_value());
        ASSERT_TRUE(second_result.has_value());
        EXPECT_TRUE((*first_result == 0U && *second_result == 3U) ||
                    (*first_result == 3U && *second_result == 0U));
        close(first);
        close(second);

        auto const exit_code{wait_for_exit(daemon_, 2s)};
        ASSERT_TRUE(exit_code.has_value());
        EXPECT_EQ(*exit_code, 70U);
        close(daemon_);
    }
    ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_FAST_CONNECT", L""), 0);
    start_daemon();
}

TEST_F(JobserverIntegration, ControlRequestTimesOutWhenHandlerIsWedged) {
    stop_daemon();
    {
        TestBarrier barrier{"before_status_response", 1};
        ASSERT_TRUE(barrier.valid());
        daemon_ = launch(JOBSERVER_DAEMON_PATH);
        ASSERT_NE(daemon_.process, nullptr);
        close_thread();
        ASSERT_TRUE(jobserver::Client::ping().has_value());

        auto const start{std::chrono::steady_clock::now()};
        auto status{jobserver::Client::status()};
        EXPECT_FALSE(status.has_value());
        if (!status) {
            EXPECT_EQ(status.error().code, "read_timeout");
        }
        EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);

        barrier.release();
        EXPECT_TRUE(jobserver::Client::ping().has_value());
        stop_daemon();
    }
    start_daemon();
}

TEST_F(JobserverIntegration, DaemonCrashAtLifecycleBarriersLeavesNoOwnershipOrProcesses) {
    stop_daemon();
    auto const helper_processes_before{process_ids_named(L"jobserver-test-helper.exe")};
    auto const marker{data_path_ / "barrier-descendant-marker.txt"};
    std::filesystem::remove(marker);
    std::vector<std::string> const phases{
        "after_admission",
        "after_resource_grant",
        "before_process_creation",
        "after_process_creation",
        "before_process_resume",
        "during_output",
        "after_root_exit_with_descendants",
        "before_resource_release",
        "during_history_recording",
    };

    for (auto index{0}; index != static_cast<int>(phases.size()); ++index) {
        auto const& phase{phases[static_cast<std::size_t>(index)]};
        TestBarrier barrier{phase, index + 10};
        ASSERT_TRUE(barrier.valid());
        start_daemon();

        auto arguments = std::vector<std::string>{"sleep", "20"};
        if (phase == "during_output") {
            arguments = {"large-output", "65536"};
        } else if (phase == "after_root_exit_with_descendants") {
            arguments = {"spawn-marker", marker.string(), "60000"};
        } else if (phase == "before_resource_release" || phase == "during_history_recording") {
            arguments = {"exit", "0"};
        }
        auto request{
            submit_request("barrier " + phase,
                           {{.name = "barrier-resource", .mode = jobserver::ClaimMode::exclusive}},
                           std::move(arguments))};
        auto client{std::async(std::launch::async, [request = std::move(request)] {
            return jobserver::Client::run(request, [](std::string const&, std::string const&) {});
        })};

        ASSERT_TRUE(barrier.wait(3s)) << phase;
        ASSERT_TRUE(TerminateProcess(daemon_.process, 91));
        ASSERT_TRUE(wait_for_exit(daemon_, 2s).has_value());
        close(daemon_);
        barrier.release();
        ASSERT_EQ(client.wait_for(2s), std::future_status::ready) << phase;
        EXPECT_FALSE(client.get().has_value()) << phase;

        for (auto attempt{0}; attempt != 50; ++attempt) {
            if (process_ids_named(L"jobserver-test-helper.exe") == helper_processes_before) {
                break;
            }
            std::this_thread::sleep_for(20ms);
        }
        EXPECT_EQ(process_ids_named(L"jobserver-test-helper.exe"), helper_processes_before)
            << phase;
    }

    EXPECT_FALSE(std::filesystem::exists(marker));
    start_daemon();
    auto status{jobserver::Client::status()};
    ASSERT_TRUE(status.has_value());
    EXPECT_TRUE(Json::parse(*status).value("jobs", Json::array()).empty());
}

TEST_F(JobserverIntegration, LaunchBoundaryCrashesKillExactProcessesAndRestartCleanly) {
    stop_daemon();
    std::vector<std::string> const phases{
        "before_process_creation",
        "before_atomic_job_assignment",
        "after_process_creation",
        "before_process_resume",
        "after_process_resume",
    };
    auto const phase_count{phases.size()};
    for (auto index{std::size_t{0}}; index != phase_count; ++index) {
        SCOPED_TRACE(phases[index]);
        {
            TestBarrier barrier{phases[index], 400 + static_cast<int>(index)};
            ASSERT_TRUE(barrier.valid());
            start_daemon();
            auto const ready{data_path_ / ("launch-ready-" + std::to_string(index))};
            auto const pipe{connect_and_handshake_raw_pipe()};
            ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
            auto request = raw_submit_message("launch crash", {"ready-sleep", ready.string()});
            request["disconnect_policy"] = index % 2 == 0 ? "cancel" : "continue";
            request["resources"] =
                Json::array({{{"name", "launch-crash-resource"}, {"mode", "exclusive"}}});
            EXPECT_TRUE(jobserver::transport::write_message(pipe, request.dump()));
            EXPECT_TRUE(barrier.wait(3s));

            auto const child_ids{helper_children_of(GetProcessId(daemon_.process))};
            auto const child_expected{index >= 2};
            EXPECT_EQ(child_ids.size(), child_expected ? 1U : 0U);
            std::vector<ChildProcess> children;
            for (auto const id : child_ids) {
                children.push_back(observe_process(id));
                EXPECT_NE(children.back().process, nullptr);
                if (children.back().process != nullptr) {
                    EXPECT_EQ(WaitForSingleObject(children.back().process, 0), WAIT_TIMEOUT);
                }
            }
            if (phases[index] == "after_process_resume") {
                EXPECT_TRUE(wait_for_file(ready, 2s));
            } else {
                EXPECT_FALSE(std::filesystem::exists(ready));
            }

            EXPECT_TRUE(TerminateProcess(daemon_.process, 92));
            EXPECT_TRUE(wait_for_exit(daemon_, 2s));
            close(daemon_);
            barrier.release();
            for (auto& child : children) {
                expect_crash_cleanup(child);
            }
            EXPECT_FALSE(jobserver::transport::read_message(pipe, 1s));
            CloseHandle(pipe);
        }

        start_daemon();
        auto status{jobserver::Client::status()};
        ASSERT_TRUE(status);
        auto const json = Json::parse(*status);
        EXPECT_TRUE(json.value("jobs", Json::array()).empty());
        EXPECT_TRUE(json.value("diagnostics", Json::array()).empty());
        EXPECT_EQ(json["daemon"].value("scheduler_entries", -1), 0);
        EXPECT_EQ(json["daemon"].value("supervised_jobs", -1), 0);
        for (auto const& resource : json.value("resources", Json::array())) {
            EXPECT_EQ(resource.value("used", -1), 0);
            EXPECT_FALSE(resource.value("exclusive", true));
        }
        auto recovery{jobserver::Client::run(
            submit_request(
                "launch recovery",
                {{.name = "launch-crash-resource", .mode = jobserver::ClaimMode::exclusive}},
                {"exit", "0"}),
            [](auto const&, auto const&) {})};
        ASSERT_TRUE(recovery);
        EXPECT_EQ(*recovery, 0);
        stop_daemon();
    }
    start_daemon();
}

TEST_F(JobserverIntegration, CrashKillsSuspendedLaunchAndConcurrentRunningTree) {
    stop_daemon();
    TestBarrier barrier{"before_process_resume", 410};
    ASSERT_TRUE(barrier.valid());
    start_daemon();
    auto const suspended_pipe{connect_and_handshake_raw_pipe()};
    ASSERT_NE(suspended_pipe, INVALID_HANDLE_VALUE);
    EXPECT_TRUE(jobserver::transport::write_message(
        suspended_pipe, raw_submit_message("suspended crash", {"sleep", "60000"}).dump()));
    EXPECT_TRUE(barrier.wait(3s));
    auto const suspended_ids{helper_children_of(GetProcessId(daemon_.process))};
    EXPECT_EQ(suspended_ids.size(), 1U);
    std::vector<ChildProcess> observed;
    for (auto const id : suspended_ids) {
        observed.push_back(observe_process(id));
    }

    auto const root_ready{data_path_ / "crash-tree-root.txt"};
    auto const child_ready{data_path_ / "crash-tree-child.txt"};
    auto const running_pipe{connect_and_handshake_raw_pipe()};
    EXPECT_NE(running_pipe, INVALID_HANDLE_VALUE);
    EXPECT_TRUE(jobserver::transport::write_message(
        running_pipe,
        raw_submit_message("running crash tree",
                           {"ready-tree", root_ready.string(), child_ready.string()})
            .dump()));
    for (auto const& ready : {root_ready, child_ready}) {
        auto const reached{wait_for_file(ready, 2s)};
        EXPECT_TRUE(reached);
        if (reached) {
            DWORD id{};
            for (auto attempt{0}; attempt != 100 && id == 0; ++attempt) {
                std::ifstream input{ready};
                input >> id;
                if (id == 0) {
                    std::this_thread::sleep_for(10ms);
                }
            }
            EXPECT_NE(id, 0U);
            observed.push_back(observe_process(id));
        }
    }
    EXPECT_EQ(observed.size(), 3U);
    for (auto const& process : observed) {
        EXPECT_NE(process.process, nullptr);
        if (process.process != nullptr) {
            EXPECT_EQ(WaitForSingleObject(process.process, 0), WAIT_TIMEOUT);
        }
    }
    EXPECT_TRUE(TerminateProcess(daemon_.process, 93));
    EXPECT_TRUE(wait_for_exit(daemon_, 2s));
    close(daemon_);
    barrier.release();
    for (auto& process : observed) {
        expect_crash_cleanup(process);
    }
    EXPECT_FALSE(jobserver::transport::read_message(suspended_pipe, 1s));
    EXPECT_FALSE(jobserver::transport::read_message(running_pipe, 1s));
    CloseHandle(suspended_pipe);
    CloseHandle(running_pipe);
    start_daemon();
    auto status{jobserver::Client::status()};
    ASSERT_TRUE(status);
    auto const json = Json::parse(*status);
    EXPECT_TRUE(json.value("jobs", Json::array()).empty());
    EXPECT_EQ(json["daemon"].value("supervised_jobs", -1), 0);
}

TEST_F(JobserverIntegration, AuditExpiresStartingJobAndReportsRecovery) {
    stop_daemon();
    ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_STARTING_TIMEOUT_MS", L"100"), 0);
    {
        TestBarrier barrier{"after_resource_grant", 100};
        ASSERT_TRUE(barrier.valid());
        start_daemon();
        auto client{std::async(std::launch::async, [] {
            return jobserver::Client::run(
                submit_request("expired starting job",
                               {{.name = "starting-timeout-resource",
                                 .mode = jobserver::ClaimMode::exclusive}},
                               {"sleep", "60000"}),
                [](std::string const&, std::string const&) {});
        })};
        ASSERT_TRUE(barrier.wait(2s));

        auto recovered{false};
        for (auto attempt{0}; attempt != 50; ++attempt) {
            auto status{jobserver::Client::status()};
            ASSERT_TRUE(status.has_value());
            auto const json = Json::parse(*status);
            auto const diagnostics = json.value("diagnostics", Json::array());
            recovered = std::ranges::any_of(diagnostics, [](Json const& diagnostic) {
                return diagnostic.get<std::string>().contains("expired STARTING");
            });
            if (recovered) {
                break;
            }
            std::this_thread::sleep_for(20ms);
        }
        EXPECT_TRUE(recovered);
        barrier.release();
        ASSERT_EQ(client.wait_for(2s), std::future_status::ready);
        EXPECT_FALSE(client.get().has_value());

        auto replacement{jobserver::Client::acquire({
            .metadata = {.name = "replacement after starting recovery",
                         .kind = "test",
                         .worktree = {}},
            .resources = {{.name = "starting-timeout-resource",
                           .mode = jobserver::ClaimMode::exclusive}},
        })};
        EXPECT_TRUE(replacement.has_value()) << replacement.error().message;
        if (replacement) {
            EXPECT_TRUE(replacement->release().has_value());
        }
        stop_daemon();
    }
    ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_STARTING_TIMEOUT_MS", L""), 0);
    start_daemon();
}

TEST_F(JobserverIntegration, UnavailablePersistenceDoesNotStrandGrantedResource) {
    stop_daemon();
    auto const blocked_data_path{data_path_ / "blocked-data"};
    {
        std::ofstream blocker{blocked_data_path};
        ASSERT_TRUE(blocker.is_open());
        blocker << "not a directory";
    }
    ASSERT_EQ(_putenv_s("NUKETHEBEES_JOBSERVER_TEST_DATA", blocked_data_path.string().c_str()), 0);
    start_daemon();

    auto result{jobserver::Client::run(
        submit_request("unavailable persistence",
                       {{.name = "persistence-resource", .mode = jobserver::ClaimMode::exclusive}},
                       {"exit", "0"}),
        [](std::string const&, std::string const&) {})};
    EXPECT_TRUE(result.has_value()) << result.error().message;
    if (result) {
        EXPECT_EQ(*result, 0);
    }
    auto const history_status{jobserver::Client::status(true)};
    ASSERT_TRUE(history_status);
    auto const history_json = Json::parse(*history_status);
    EXPECT_EQ(history_json["daemon"]["scheduler_entries"], 0U);
    EXPECT_TRUE(std::ranges::any_of(history_json["jobs"], [](auto const& job) {
        return job.value("name", "") == "unavailable persistence" &&
               job.value("state", "") == "SUCCEEDED";
    }));
    auto replacement{jobserver::Client::acquire({
        .metadata = {.name = "replacement after persistence failure",
                     .kind = "test",
                     .worktree = {}},
        .resources = {{.name = "persistence-resource", .mode = jobserver::ClaimMode::exclusive}},
    })};
    EXPECT_TRUE(replacement.has_value()) << replacement.error().message;
    if (replacement) {
        EXPECT_TRUE(replacement->release().has_value());
    }

    stop_daemon();
    ASSERT_EQ(_putenv_s("NUKETHEBEES_JOBSERVER_TEST_DATA", data_path_.string().c_str()), 0);
    std::filesystem::remove(blocked_data_path);
    start_daemon();
}

TEST_F(JobserverIntegration, CrashedLeaseClientReleasesItsResource) {
    auto client{launch(JOBSERVER_TEST_CLIENT_PATH, L"lease-crash")};
    ASSERT_NE(client.process, nullptr);
    auto const exit_code{wait_for_exit(client, 5s)};
    ASSERT_TRUE(exit_code.has_value());
    ASSERT_EQ(*exit_code, 99U);
    close(client);

    auto lease{jobserver::Client::acquire({
        .metadata = {.name = "replacement lease", .kind = "test", .worktree = {}},
        .resources = {{.name = "crash-resource", .mode = jobserver::ClaimMode::exclusive}},
    })};
    ASSERT_TRUE(lease.has_value()) << lease.error().message;
}

TEST_F(JobserverIntegration, QueuedClientReceivesPeriodicHeartbeat) {
    auto active{jobserver::Client::acquire({
        .metadata = {.name = "heartbeat blocker", .kind = "test", .worktree = {}},
        .resources = {{.name = "heartbeat-resource", .mode = jobserver::ClaimMode::exclusive}},
    })};
    ASSERT_TRUE(active.has_value());
    auto pipe{connect_and_handshake_raw_pipe()};
    ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
    auto message = raw_submit_message("heartbeat waiter", {"exit", "0"});
    message["resources"] = Json::array();
    message["resources"].push_back(
        Json{{"name", "heartbeat-resource"}, {"mode", "exclusive"}, {"units", 1}});
    ASSERT_TRUE(jobserver::transport::write_message(pipe, message.dump()).has_value());

    auto heartbeat{jobserver::transport::read_message(pipe, 1s)};
    ASSERT_TRUE(heartbeat.has_value()) << heartbeat.error().message;
    auto const heartbeat_json = Json::parse(*heartbeat);
    ASSERT_TRUE(heartbeat_json.is_object()) << heartbeat_json.dump();
    EXPECT_EQ(heartbeat_json.value("type", ""), "queued");

    CloseHandle(pipe);
    EXPECT_TRUE(active->release().has_value());
    for (auto attempt{0}; attempt != 100; ++attempt) {
        auto status{jobserver::Client::status()};
        ASSERT_TRUE(status.has_value());
        auto const jobs = Json::parse(*status).value("jobs", Json::array());
        if (std::ranges::none_of(jobs, [](Json const& job) {
                return job.value("name", "") == "heartbeat waiter";
            })) {
            return;
        }
        std::this_thread::sleep_for(20ms);
    }
    FAIL() << "Heartbeat waiter did not drain after its client disconnected";
}

TEST_F(JobserverIntegration, GrantBetweenQueuePollAndStateReadIsNotCancellation) {
    stop_daemon();
    for (auto const lease_mode : {false, true}) {
        TestBarrier barrier{"after_queue_poll", lease_mode ? 302 : 301};
        ASSERT_TRUE(barrier.valid());
        start_daemon();
        auto owner{jobserver::Client::acquire({
            .metadata = {.name = "queue poll owner", .kind = "test", .worktree = {}},
            .resources = {{.name = "queue-poll-race", .mode = jobserver::ClaimMode::exclusive}},
        })};
        ASSERT_TRUE(owner);
        auto waiting{
            std::async(std::launch::async, [lease_mode]() -> std::expected<int, jobserver::Error> {
                if (lease_mode) {
                    auto lease{jobserver::Client::acquire({
                        .metadata = {.name = "queue poll waiter", .kind = "test", .worktree = {}},
                        .resources = {{.name = "queue-poll-race",
                                       .mode = jobserver::ClaimMode::exclusive}},
                    })};
                    if (!lease) {
                        return std::unexpected(lease.error());
                    }
                    if (auto released{lease->release()}; !released) {
                        return std::unexpected(released.error());
                    }
                    return 0;
                }
                return jobserver::Client::run(
                    submit_request(
                        "queue poll waiter",
                        {{.name = "queue-poll-race", .mode = jobserver::ClaimMode::exclusive}},
                        {"exit", "0"}),
                    [](auto const&, auto const&) {});
            })};
        auto const reached{barrier.wait(2s)};
        EXPECT_TRUE(owner->release());
        auto const starting{find_job_in_state("queue poll waiter", "STARTING")};
        barrier.release();
        EXPECT_TRUE(reached);
        EXPECT_TRUE(starting);
        EXPECT_EQ(waiting.wait_for(3s), std::future_status::ready);
        auto const result{waiting.get()};
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_EQ(*result, 0);
        stop_daemon();
    }
    start_daemon();
}

TEST_F(JobserverIntegration, AuditSnapshotCannotInvalidateNewOwners) {
    stop_daemon();
    TestBarrier barrier{"after_audit_owner_snapshot", 303};
    ASSERT_TRUE(barrier.valid());
    start_daemon();
    auto anchor{jobserver::Client::acquire({
        .metadata = {.name = "audit anchor", .kind = "test", .worktree = {}},
        .resources = {{.name = "audit-anchor-resource", .mode = jobserver::ClaimMode::exclusive}},
    })};
    ASSERT_TRUE(anchor);
    ASSERT_TRUE(barrier.wait(2s));

    auto lease_future{std::async(std::launch::async, [] {
        return jobserver::Client::acquire(
            test_request("audit lease", jobserver::ClaimMode::exclusive));
    })};
    auto process_future{std::async(std::launch::async, [] {
        return jobserver::Client::run(submit_request("audit process",
                                                     {{.name = "audit-process-resource",
                                                       .mode = jobserver::ClaimMode::exclusive}},
                                                     {"sleep", "1000"}),
                                      [](auto const&, auto const&) {});
    })};
    std::this_thread::sleep_for(200ms);
    auto const early_grant{lease_future.wait_for(0ms)};
    barrier.release();
    EXPECT_TRUE(anchor->release());
    EXPECT_EQ(early_grant, std::future_status::timeout);
    ASSERT_EQ(lease_future.wait_for(2s), std::future_status::ready);
    auto lease{lease_future.get()};
    ASSERT_TRUE(lease) << lease.error().message;
    EXPECT_TRUE(find_job_in_state("audit lease", "RUNNING"));
    EXPECT_TRUE(lease->release());
    ASSERT_EQ(process_future.wait_for(3s), std::future_status::ready);
    auto result{process_future.get()};
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(*result, 0);
    auto status{jobserver::Client::status()};
    ASSERT_TRUE(status);
    auto const json = Json::parse(*status);
    EXPECT_TRUE(json.value("diagnostics", Json::array()).empty()) << json.dump();
    EXPECT_EQ(json["daemon"].value("scheduler_entries", -1), 0);
    EXPECT_EQ(json["daemon"].value("supervised_jobs", -1), 0);
    EXPECT_EQ(json["daemon"].value("leases", -1), 0);
}

TEST_F(JobserverIntegration, ConflictingClientsAreGrantedInFifoOrder) {
    auto active{
        jobserver::Client::acquire(test_request("active shared", jobserver::ClaimMode::shared))};
    ASSERT_TRUE(active.has_value());
    auto exclusive_future{std::async(std::launch::async, [] {
        return jobserver::Client::acquire(
            test_request("waiting exclusive", jobserver::ClaimMode::exclusive));
    })};
    ASSERT_TRUE(find_job_in_state("waiting exclusive", "QUEUED").has_value());
    auto later_future{std::async(std::launch::async, [] {
        return jobserver::Client::acquire(
            test_request("later shared", jobserver::ClaimMode::shared));
    })};
    ASSERT_TRUE(find_job_in_state("later shared", "QUEUED").has_value());

    ASSERT_TRUE(active->release().has_value());
    ASSERT_EQ(exclusive_future.wait_for(2s), std::future_status::ready);
    auto exclusive{exclusive_future.get()};
    ASSERT_TRUE(exclusive.has_value());
    EXPECT_EQ(later_future.wait_for(200ms), std::future_status::timeout);
    ASSERT_TRUE(exclusive->release().has_value());
    ASSERT_EQ(later_future.wait_for(2s), std::future_status::ready);
    EXPECT_TRUE(later_future.get().has_value());
}

TEST_F(JobserverIntegration, QueuedCancellationCompletesTheSubmittingClient) {
    auto active{jobserver::Client::acquire(
        test_request("active exclusive", jobserver::ClaimMode::exclusive))};
    ASSERT_TRUE(active.has_value());
    auto run_future{std::async(std::launch::async, [] {
        return jobserver::Client::run(
            {
                .metadata = {.name = "cancelled queued job", .kind = "test", .worktree = {}},
                .command = {.executable = JOBSERVER_TEST_HELPER_PATH,
                            .arguments = {"sleep", "60000"},
                            .working_directory =
                                std::filesystem::path{JOBSERVER_TEST_HELPER_PATH}.parent_path(),
                            .environment = {}},
                .resources = {{.name = "integration-resource",
                               .mode = jobserver::ClaimMode::exclusive}},
                .timeout = std::nullopt,
                .suspect_after = std::nullopt,
                .disconnect_policy = jobserver::DisconnectPolicy::cancel,
            },
            [](std::string const&, std::string const&) {});
    })};
    auto const queued{find_job("cancelled queued job")};
    if (!queued) {
        if (run_future.wait_for(0ms) == std::future_status::ready) {
            auto const early_result{run_future.get()};
            ASSERT_TRUE(early_result.has_value()) << early_result.error().message;
            FAIL() << "Queued command completed unexpectedly with exit code " << *early_result;
        }
        FAIL() << "Queued command did not appear in daemon status";
    }
    auto cancel{jobserver::Client::cancel(queued->value("id", ""), false)};
    ASSERT_TRUE(cancel.has_value()) << cancel.error().message;
    ASSERT_EQ(run_future.wait_for(2s), std::future_status::ready);
    auto const result{run_future.get()};
    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(*result, 130);
}

TEST_F(JobserverIntegration, ShutdownAndAcquireCannotBothCommit) {
    for (auto iteration{0}; iteration != 10; ++iteration) {
        ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_FAST_CONNECT", L"1"), 0);
        std::atomic<bool> begin{};
        auto acquire_future{std::async(std::launch::async, [&] {
            while (!begin.load()) {
                std::this_thread::yield();
            }
            return jobserver::Client::acquire(
                test_request("racing acquire", jobserver::ClaimMode::exclusive));
        })};
        auto shutdown_future{std::async(std::launch::async, [&] {
            while (!begin.load()) {
                std::this_thread::yield();
            }
            return jobserver::Client::shutdown();
        })};
        begin.store(true);
        auto acquire{acquire_future.get()};
        auto shutdown{shutdown_future.get()};
        ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_FAST_CONNECT", L""), 0);
        EXPECT_FALSE(acquire.has_value() && shutdown.has_value());

        if (acquire) {
            ASSERT_FALSE(shutdown.has_value());
            ASSERT_TRUE(acquire->release().has_value());
        }
        if (!shutdown) {
            ASSERT_TRUE(jobserver::Client::shutdown().has_value());
        }
        ASSERT_TRUE(wait_for_exit(daemon_, 5s).has_value());
        close(daemon_);
        if (iteration != 9) {
            start_daemon();
        }
    }
}

TEST_F(JobserverIntegration, IndependentWorktreeProcessesShareOneQueue) {
    auto const first_worktree{data_path_ / "worktree one"};
    auto const second_worktree{data_path_ / "worktree two"};
    std::filesystem::create_directories(first_worktree);
    std::filesystem::create_directories(second_worktree);
    auto first{launch(JOBSERVER_TEST_CLIENT_PATH,
                      L"lease-hold worktree-one cross-worktree-resource 2000",
                      first_worktree)};
    ASSERT_NE(first.process, nullptr);
    auto const first_job{find_job("worktree-one")};
    ASSERT_TRUE(first_job.has_value());
    EXPECT_EQ(first_job->value("worktree", ""), jobserver::path_to_utf8(first_worktree));
    EXPECT_EQ(first_job->value("submit_directory", ""), jobserver::path_to_utf8(first_worktree));

    auto second{launch(JOBSERVER_TEST_CLIENT_PATH,
                       L"lease-hold worktree-two cross-worktree-resource 50",
                       second_worktree)};
    ASSERT_NE(second.process, nullptr);
    auto const second_job{find_job("worktree-two")};
    ASSERT_TRUE(second_job.has_value());
    EXPECT_EQ(second_job->value("state", ""), "QUEUED");
    EXPECT_EQ(second_job->value("worktree", ""), jobserver::path_to_utf8(second_worktree));
    EXPECT_EQ(second_job->value("submit_directory", ""), jobserver::path_to_utf8(second_worktree));

    auto const first_exit{wait_for_exit(first, 5s)};
    auto const second_exit{wait_for_exit(second, 5s)};
    ASSERT_TRUE(first_exit.has_value());
    ASSERT_TRUE(second_exit.has_value());
    EXPECT_EQ(*first_exit, 0U);
    EXPECT_EQ(*second_exit, 0U);
    close(first);
    close(second);
}

TEST_F(JobserverIntegration, CliSubmissionPublishesDistinctWorktreeAndSubmissionDirectory) {
    auto const submit_directory{data_path_ / "submitted from"};
    auto const worktree{data_path_ / "logical worktree"};
    std::filesystem::create_directories(submit_directory);
    std::filesystem::create_directories(worktree);

    auto const name{"CLI provenance"};
    auto const arguments{L"run --name \"CLI provenance\" --kind test --worktree " +
                         quote(worktree.wstring()) + L" --shared provenance-resource -- " +
                         quote(std::filesystem::path{JOBSERVER_TEST_HELPER_PATH}.wstring()) +
                         L" sleep 60000"};
    auto client{launch(JOBSERVER_CLI_PATH, arguments, submit_directory)};
    ASSERT_NE(client.process, nullptr);

    auto const active{find_job(name)};
    ASSERT_TRUE(active.has_value());
    EXPECT_EQ(active->value("worktree", ""), jobserver::path_to_utf8(worktree));
    EXPECT_EQ(active->value("submit_directory", ""), jobserver::path_to_utf8(submit_directory));

    auto const status_json{run_and_capture(JOBSERVER_CLI_PATH, L"status --json")};
    ASSERT_TRUE(status_json.has_value());
    ASSERT_EQ(status_json->exit_code, 0U);
    auto const live_status = Json::parse(status_json->output);
    auto const live_job{std::ranges::find_if(live_status["jobs"], [&](Json const& job) {
        return job.value("id", "") == active->value("id", "");
    })};
    ASSERT_NE(live_job, live_status["jobs"].end());
    EXPECT_EQ(live_job->value("worktree", ""), jobserver::path_to_utf8(worktree));
    EXPECT_EQ(live_job->value("submit_directory", ""), jobserver::path_to_utf8(submit_directory));

    auto const text_status{run_and_capture(JOBSERVER_CLI_PATH, L"status")};
    ASSERT_TRUE(text_status.has_value());
    EXPECT_EQ(text_status->exit_code, 0U);
    EXPECT_NE(text_status->output.find("worktree: " + jobserver::path_to_utf8(worktree)),
              std::string::npos);
    EXPECT_NE(
        text_status->output.find("submitted-from: " + jobserver::path_to_utf8(submit_directory)),
        std::string::npos);

    auto const id{active->value("id", "")};
    auto const show{
        run_and_capture(JOBSERVER_CLI_PATH, L"show " + std::wstring{id.begin(), id.end()})};
    ASSERT_TRUE(show.has_value());
    ASSERT_EQ(show->exit_code, 0U);
    auto const shown = Json::parse(show->output);
    EXPECT_EQ(shown.value("worktree", ""), jobserver::path_to_utf8(worktree));
    EXPECT_EQ(shown.value("submit_directory", ""), jobserver::path_to_utf8(submit_directory));

    EXPECT_TRUE(jobserver::Client::cancel(id, true));
    EXPECT_TRUE(wait_for_exit(client, 5s).has_value());
    close(client);

    auto const history_json{run_and_capture(JOBSERVER_CLI_PATH, L"history --json")};
    ASSERT_TRUE(history_json.has_value());
    ASSERT_EQ(history_json->exit_code, 0U);
    auto const historical = Json::parse(history_json->output);
    auto const completed{std::ranges::find_if(
        historical["jobs"], [&](Json const& job) { return job.value("name", "") == name; })};
    ASSERT_NE(completed, historical["jobs"].end());
    EXPECT_EQ(completed->value("worktree", ""), jobserver::path_to_utf8(worktree));
    EXPECT_EQ(completed->value("submit_directory", ""), jobserver::path_to_utf8(submit_directory));

    auto const text_history{run_and_capture(JOBSERVER_CLI_PATH, L"history")};
    ASSERT_TRUE(text_history.has_value());
    EXPECT_EQ(text_history->exit_code, 0U);
    EXPECT_NE(text_history->output.find("worktree: " + jobserver::path_to_utf8(worktree)),
              std::string::npos);
    EXPECT_NE(
        text_history->output.find("submitted-from: " + jobserver::path_to_utf8(submit_directory)),
        std::string::npos);
}

TEST_F(JobserverIntegration, NestedCommandInheritsInvokingBuildDirectory) {
    auto const build_directory{data_path_ / "nested-build-directory"};
    auto const worktree{data_path_ / "nested-worktree"};
    auto const marker{std::filesystem::path{"nested-marker.txt"}};
    std::filesystem::create_directories(build_directory);
    std::filesystem::create_directories(worktree);

    auto request{submit_request("nested directory",
                                {{.name = "machine", .mode = jobserver::ClaimMode::exclusive}},
                                {"run",
                                 "--worktree",
                                 jobserver::path_to_utf8(worktree),
                                 "--shared",
                                 "machine",
                                 "--",
                                 JOBSERVER_TEST_HELPER_PATH,
                                 "marker-after",
                                 marker.string(),
                                 "0"})};
    request.command.executable = JOBSERVER_CLI_PATH;
    request.command.working_directory = build_directory;
    auto const result{jobserver::Client::run(request, [](auto const&, auto const&) {})};
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(*result, 0);
    EXPECT_TRUE(std::filesystem::exists(build_directory / marker));
    EXPECT_FALSE(std::filesystem::exists(worktree / marker));
}

TEST_F(JobserverIntegration, NestedCommandDoesNotCreateAConsoleWindow) {
    auto request{submit_request("nested console",
                                {{.name = "machine", .mode = jobserver::ClaimMode::exclusive}},
                                {"nested-run", "shared", "machine", "1", "require-no-console"})};
    request.command.executable = JOBSERVER_TEST_CLIENT_PATH;
    auto const result{jobserver::Client::run(request, [](auto const&, auto const&) {})};
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(*result, 0);
}

TEST_F(JobserverIntegration, NestedClaimsAreValidatedBeforeLaunching) {
    for (auto const mode : {jobserver::ClaimMode::shared, jobserver::ClaimMode::exclusive}) {
        auto const marker{data_path_ / (jobserver::to_string(mode) + "-nested.txt")};
        auto request{submit_request(
            "nested claims",
            {{.name = "machine", .mode = mode}},
            {"nested-run", "exclusive", "machine", "1", "marker-after", marker.string(), "0"})};
        request.command.executable = JOBSERVER_TEST_CLIENT_PATH;
        auto const result{jobserver::Client::run(request, [](auto const&, auto const&) {})};
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_EQ(*result, mode == jobserver::ClaimMode::exclusive ? 0 : 124);
        EXPECT_EQ(std::filesystem::exists(marker), mode == jobserver::ClaimMode::exclusive);
    }
    auto request{
        submit_request("nested no claims", {}, {"nested-run", "shared", "none", "1", "exit", "0"})};
    request.command.executable = JOBSERVER_TEST_CLIENT_PATH;
    auto const result{jobserver::Client::run(request, [](auto const&, auto const&) {})};
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 0);
}

TEST_F(JobserverIntegration, NestedOutputFlowsThroughParentCapture) {
    auto request{submit_request("nested output",
                                {{.name = "machine", .mode = jobserver::ClaimMode::exclusive}},
                                {"nested-run", "shared", "machine", "1", "output", "2"})};
    request.command.executable = JOBSERVER_TEST_CLIENT_PATH;
    std::string standard_output;
    std::string standard_error;
    auto const result{
        jobserver::Client::run(request, [&](std::string const& stream, std::string const& data) {
            (stream == "stdout" ? standard_output : standard_error) += data;
        })};
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(*result, 0);
    EXPECT_NE(standard_output.find("stdout 1"), std::string::npos);
    EXPECT_NE(standard_error.find("stderr 1"), std::string::npos);
}

TEST_F(JobserverIntegration, CompletedJobsLeaveLiveTableButRemainInHistory) {
    for (auto index{0}; index < 32; ++index) {
        auto const result{
            jobserver::Client::run(submit_request("retired short job", {}, {"exit", "0"}),
                                   [](auto const&, auto const&) {})};
        ASSERT_TRUE(result);
        EXPECT_EQ(*result, 0);
    }
    auto const status{jobserver::Client::status(true)};
    ASSERT_TRUE(status);
    auto const json = Json::parse(*status);
    EXPECT_EQ(json["daemon"]["scheduler_entries"], 0U);
    auto count{0};
    for (auto const& job : json["jobs"]) {
        if (job.value("name", "") == "retired short job") {
            EXPECT_EQ(job.value("state", ""), "SUCCEEDED");
            ++count;
        }
    }
    EXPECT_EQ(count, 32);
}

TEST_F(JobserverIntegration, DiskLogCapDoesNotTruncateClientOutput) {
    auto const bytes{jobserver::LogStore::maximum_stream_bytes + 1024U};
    std::size_t received{};
    auto const result{jobserver::Client::run(
        submit_request("capped output", {}, {"large-output", std::to_string(bytes)}),
        [&](auto const& stream, auto const& text) {
            if (stream == "stdout") {
                received += text.size();
            }
        })};
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 0);
    EXPECT_EQ(received, bytes);
    auto const status{jobserver::Client::status(true)};
    ASSERT_TRUE(status);
    auto const json = Json::parse(*status);
    auto const found{std::ranges::find_if(
        json["jobs"], [](auto const& job) { return job.value("name", "") == "capped output"; })};
    ASSERT_NE(found, json["jobs"].end());
    auto const path{data_path_ / "logs" / (found->value("id", "") + ".stdout.log")};
    EXPECT_EQ(std::filesystem::file_size(path), jobserver::LogStore::maximum_stream_bytes);
    std::ifstream input{path, std::ios::binary};
    input.seekg(-36, std::ios::end);
    std::string const tail{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    EXPECT_NE(tail.find("log size limit reached"), std::string::npos);
}

TEST_F(JobserverIntegration, NestedClaimsRejectSpoofedAndStaleParentIds) {
    auto parent{std::async(std::launch::async, [] {
        return jobserver::Client::run(submit_request("spoof parent",
                                                     {{.name = "integration-resource",
                                                       .mode = jobserver::ClaimMode::exclusive}},
                                                     {"sleep", "60000"},
                                                     5s),
                                      [](auto const&, auto const&) {});
    })};
    auto const running{find_job_in_state("spoof parent", "RUNNING")};
    ASSERT_TRUE(running);
    auto const status{jobserver::Client::status()};
    ASSERT_TRUE(status);
    auto const jobs = Json::parse(*status)["jobs"];
    ASSERT_EQ(jobs.size(), 1U);
    EXPECT_EQ(jobs[0]["claims"][0]["name"], "integration-resource");
    auto request{
        submit_request("spoofed nested",
                       {{.name = "integration-resource", .mode = jobserver::ClaimMode::exclusive}},
                       {"exit", "0"})};
    auto const id{jobs[0]["id"].get<std::string>()};
    ASSERT_EQ(_putenv_s("NUKETHEBEES_JOBSERVER_JOB", id.c_str()), 0);
    auto const spoofed{jobserver::Client::run(request, [](auto const&, auto const&) {})};
    ASSERT_EQ(_putenv_s("NUKETHEBEES_JOBSERVER_JOB", "unknown"), 0);
    auto const stale{jobserver::Client::run(request, [](auto const&, auto const&) {})};
    ASSERT_EQ(_putenv_s("NUKETHEBEES_JOBSERVER_JOB", ""), 0);
    EXPECT_TRUE(jobserver::Client::cancel(id, true));
    EXPECT_EQ(parent.wait_for(3s), std::future_status::ready);
    EXPECT_TRUE(parent.get());
    ASSERT_FALSE(spoofed);
    EXPECT_EQ(spoofed.error().code, "nested_parent_mismatch");
    ASSERT_FALSE(stale);
    EXPECT_EQ(stale.error().code, "nested_parent_not_active");
}

TEST_F(JobserverIntegration, SubmittedAndNestedCommandsApplyEnvironmentChanges) {
    for (auto const nested : {false, true}) {
        auto request{submit_request("environment changes",
                                    {},
                                    nested ? std::vector<std::string>{"nested-environment"}
                                           : std::vector<std::string>{"check-environment"})};
        request.command.environment = {
            {.name = "JOBSERVER_ENV_ADD", .value = "added"},
            {.name = "JOBSERVER_ENV_REPLACE", .value = "original"},
            {.name = "jobserver_env_replace", .value = nested ? "original" : "replaced"},
            {.name = "JOBSERVER_ENV_REMOVE", .value = "remove me"},
            {.name = "NUKETHEBEES_JOBSERVER_JOB", .value = "spoofed"}};
        if (nested) {
            request.command.executable = JOBSERVER_TEST_CLIENT_PATH;
        } else {
            request.command.environment.push_back(
                {.name = "JOBSERVER_ENV_REMOVE", .value = std::nullopt});
        }
        auto const result{jobserver::Client::run(request, [](auto const&, auto const&) {})};
        ASSERT_TRUE(result) << result.error().message;
        EXPECT_EQ(*result, 0);
    }
}

TEST_F(JobserverIntegration, InvalidEnvironmentDoesNotLaunchOrStrandResources) {
    auto request{submit_request("invalid environment",
                                {{.name = "machine", .mode = jobserver::ClaimMode::exclusive}},
                                {"exit", "0"})};
    for (auto const& name : {std::string{}, std::string{"bad=name"}, std::string{"bad\0name", 8}}) {
        request.command.environment = {{.name = name, .value = "value"}};
        auto const result{jobserver::Client::run(request, [](auto const&, auto const&) {})};
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, "invalid_environment");
    }
    request.command.environment.clear();
    auto const result{jobserver::Client::run(request, [](auto const&, auto const&) {})};
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 0);
}

TEST_F(JobserverIntegration, CancellingParentTerminatesValidatedNestedChild) {
    auto const ready_path{data_path_ / "nested-child-ready.txt"};
    auto request{
        submit_request("nested cancellation",
                       {{.name = "machine", .mode = jobserver::ClaimMode::exclusive}},
                       {"nested-run", "shared", "machine", "1", "ready-sleep", ready_path.string()},
                       5s)};
    request.command.executable = JOBSERVER_TEST_CLIENT_PATH;
    auto result{std::async(std::launch::async, [request] {
        return jobserver::Client::run(request, [](auto const&, auto const&) {});
    })};
    auto const parent{find_job_in_state("nested cancellation", "RUNNING")};
    ASSERT_TRUE(parent);
    DWORD process_id{};
    auto const deadline{std::chrono::steady_clock::now() + 3s};
    while (process_id == 0 && std::chrono::steady_clock::now() < deadline) {
        std::ifstream ready{ready_path};
        ready >> process_id;
        std::this_thread::sleep_for(10ms);
    }
    auto const child{process_id == 0 ? nullptr : OpenProcess(SYNCHRONIZE, FALSE, process_id)};
    EXPECT_NE(child, nullptr);
    EXPECT_TRUE(jobserver::Client::cancel(parent->value("id", ""), true));
    if (child != nullptr) {
        EXPECT_EQ(WaitForSingleObject(child, 3000), WAIT_OBJECT_0);
        CloseHandle(child);
    }
    EXPECT_EQ(result.wait_for(3s), std::future_status::ready);
    EXPECT_TRUE(result.get());
    EXPECT_TRUE(wait_for_job_to_disappear("nested cancellation", 3s));
}

TEST_F(JobserverIntegration, CliResolvesExecutableFromSubmittingProcessPath) {
    auto const helper_directory{std::filesystem::path{JOBSERVER_TEST_HELPER_PATH}.parent_path()};
    auto const required{GetEnvironmentVariableW(L"PATH", nullptr, 0)};
    ASSERT_NE(required, 0U);
    std::wstring original_path(static_cast<std::size_t>(required), L'\0');
    auto const written{
        GetEnvironmentVariableW(L"PATH", original_path.data(), static_cast<DWORD>(required))};
    ASSERT_NE(written, 0U);
    original_path.resize(written);

    auto const test_path{helper_directory.wstring() + L";" + original_path};
    ASSERT_TRUE(SetEnvironmentVariableW(L"PATH", test_path.c_str()));
    auto child{launch(JOBSERVER_CLI_PATH,
                      L"run --name path-resolution --kind test -- "
                      L"jobserver-test-helper exit 0",
                      data_path_)};
    auto const restore_succeeded{SetEnvironmentVariableW(L"PATH", original_path.c_str()) != FALSE};
    ASSERT_TRUE(restore_succeeded);

    ASSERT_NE(child.process, nullptr);
    auto const exit_code{wait_for_exit(child, 3s)};
    ASSERT_TRUE(exit_code.has_value());
    EXPECT_EQ(*exit_code, 0U);
    close(child);
}

TEST_F(JobserverIntegration, RejectsProtocolMismatchAndMalformedRequestWithoutStopping) {
    auto pipe{connect_raw_pipe()};
    ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
    auto const mismatch =
        Json{{"type", "hello"},
             {"protocol", {{"major", jobserver::protocol::major_version + 1}, {"minor", 0}}}};
    ASSERT_TRUE(jobserver::transport::write_message(pipe, mismatch.dump()).has_value());
    auto response{jobserver::transport::read_message(pipe)};
    ASSERT_TRUE(response.has_value());
    auto const mismatch_response = Json::parse(*response);
    EXPECT_EQ(mismatch_response.value("code", ""), "protocol_mismatch");
    CloseHandle(pipe);

    pipe = connect_raw_pipe();
    ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
    auto const hello = Json{{"type", "hello"},
                            {"protocol",
                             {{"major", jobserver::protocol::major_version},
                              {"minor", jobserver::protocol::minor_version}}}};
    ASSERT_TRUE(jobserver::transport::write_message(pipe, hello.dump()).has_value());
    ASSERT_TRUE(jobserver::transport::read_message(pipe).has_value());
    ASSERT_TRUE(jobserver::transport::write_message(pipe, "{").has_value());
    response = jobserver::transport::read_message(pipe);
    ASSERT_TRUE(response.has_value());
    auto const malformed_response = Json::parse(*response);
    EXPECT_EQ(malformed_response.value("code", ""), "invalid_json");
    CloseHandle(pipe);
    EXPECT_TRUE(jobserver::Client::status().has_value());
}

TEST_F(JobserverIntegration, MalformedFieldsFailBeforeAdmissionOrExecution) {
    auto const marker{data_path_ / "malformed-command-must-not-run.txt"};
    auto base = raw_submit_message("malformed fields", {"marker-after", marker.string(), "0"});
    base["resources"] =
        Json::array({{{"name", "malformed-resource"}, {"mode", "counted"}, {"units", 1}}});
    base["command"]["environment"] = Json::array({{{"name", "TEST_VALUE"}, {"value", "valid"}}});
    std::vector<std::pair<std::string, Json>> const cases{
        {"/type", nullptr},
        {"/type", 1},
        {"/type", true},
        {"/type", ""},
        {"/metadata", nullptr},
        {"/metadata", Json::array()},
        {"/metadata/name", 1},
        {"/metadata/kind", false},
        {"/metadata/worktree", Json::object()},
        {"/metadata/submit_directory", Json::array()},
        {"/resources", nullptr},
        {"/resources", Json::object()},
        {"/resources/0", nullptr},
        {"/resources/0", "resource"},
        {"/resources/0/name", 1},
        {"/resources/0/mode", false},
        {"/resources/0/mode", "invalid"},
        {"/resources/0/units", "1"},
        {"/resources/0/units", 1.5},
        {"/resources/0/units", -1},
        {"/resources/0/units", 0},
        {"/resources/0/units", 4294967297ULL},
        {"/command", nullptr},
        {"/command", Json::array()},
        {"/command/executable", false},
        {"/command/executable", ""},
        {"/command/executable", std::string("bad\0path", 8)},
        {"/command/arguments", "argument"},
        {"/command/arguments/0", nullptr},
        {"/command/arguments/0", std::string("bad\0argument", 12)},
        {"/command/working_directory", 1},
        {"/command/environment", false},
        {"/command/environment/0", nullptr},
        {"/command/environment/0/name", false},
        {"/command/environment/0/name", ""},
        {"/command/environment/0/name", "bad=name"},
        {"/command/environment/0/value", Json::object()},
        {"/disconnect_policy", false},
        {"/disconnect_policy", "invalid"},
        {"/timeout_ms", nullptr},
        {"/timeout_ms", "100"},
        {"/timeout_ms", false},
        {"/timeout_ms", 0},
        {"/timeout_ms", -1},
        {"/timeout_ms", 1.5},
        {"/timeout_ms", 18446744073709551615ULL},
        {"/suspect_after_ms", Json::array()},
        {"/suspect_after_ms", -1},
        {"/suspect_after_ms", 18446744073709551615ULL},
    };
    auto check = [&](Json const& request) {
        SCOPED_TRACE(request.dump());
        auto const pipe{connect_and_handshake_raw_pipe()};
        ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
        auto const sent{jobserver::transport::write_message(pipe, request.dump(), 1s)};
        auto const response{
            sent ? jobserver::transport::read_message(pipe, 1s)
                 : std::expected<std::string, jobserver::Error>{std::unexpected(sent.error())}};
        CloseHandle(pipe);
        ASSERT_TRUE(response) << response.error().message;
        auto const json = Json::parse(*response, nullptr, false);
        ASSERT_TRUE(json.is_object());
        EXPECT_EQ(json.value("type", ""), "error");
        EXPECT_FALSE(json.value("code", "").empty());
        EXPECT_FALSE(json.value("message", "").empty());
        EXPECT_FALSE(std::filesystem::exists(marker));
        EXPECT_TRUE(jobserver::Client::ping());
        auto const status{jobserver::Client::status()};
        ASSERT_TRUE(status);
        auto const state = Json::parse(*status);
        EXPECT_EQ(state["daemon"]["scheduler_entries"], 0U);
        EXPECT_EQ(state["daemon"]["supervised_jobs"], 0U);
        EXPECT_EQ(state["daemon"]["leases"], 0U);
        EXPECT_TRUE(state["jobs"].empty());
        for (auto const& resource : state["resources"]) {
            EXPECT_EQ(resource.value("used", 0U), 0U);
            EXPECT_FALSE(resource.value("exclusive", false));
        }
    };
    for (auto const& [path, value] : cases) {
        auto request = base;
        request[Json::json_pointer{path}] = value;
        check(request);
    }
    for (auto const path : {"/type",
                            "/command",
                            "/command/executable",
                            "/resources/0/name",
                            "/resources/0/mode",
                            "/command/environment/0/name",
                            "/command/environment/0/value"}) {
        auto request = base;
        auto const pointer{Json::json_pointer{path}};
        request[pointer.parent_pointer()].erase(pointer.back());
        check(request);
    }
    check(Json{{"type", "status"}, {"history", "true"}});
    check(Json{{"type", "cancel"}, {"id", false}});
    check(Json{{"type", "kill"}, {"id", Json::array()}});
    check(Json{{"type", "validate_nested"}, {"parent_id", 1}});
    check(Json{{"type", "cancel"}});
    check(Json{{"type", "unexpected_message"}});
    check(Json::array());
    check(Json(nullptr));
    check(Json(true));
    for (auto const type : {"acquire", "validate_nested"}) {
        for (auto const& [path, value] : cases) {
            if (path.starts_with("/resources") ||
                (std::string_view{type} == "acquire" && path.starts_with("/metadata"))) {
                auto request = base;
                request["type"] = type;
                request["parent_id"] = "unknown";
                request[Json::json_pointer{path}] = value;
                check(request);
            }
        }
    }
}

TEST_F(JobserverIntegration, MalformedHandshakeVersionsNeverWrapOrCoerce) {
    auto check = [&](Json const& hello) {
        auto const pipe{connect_raw_pipe()};
        ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
        SCOPED_TRACE(hello.dump());
        ASSERT_TRUE(jobserver::transport::write_message(pipe, hello.dump(), 1s));
        auto const response{jobserver::transport::read_message(pipe, 1s)};
        CloseHandle(pipe);
        ASSERT_TRUE(response);
        auto const json = Json::parse(*response);
        EXPECT_EQ(json.value("type", ""), "error");
        EXPECT_EQ(json.value("code", ""), "protocol_mismatch");
        EXPECT_TRUE(jobserver::Client::ping());
    };
    for (auto const field : {"major", "minor"}) {
        for (auto const& value : std::vector<Json>{nullptr, true, "1", 1.5, -1, 4294967297ULL}) {
            auto hello = Json{{"type", "hello"}, {"protocol", {{"major", 1}, {"minor", 0}}}};
            hello["protocol"][field] = value;
            check(hello);
        }
    }
    check(Json::array());
    check(Json{{"type", true}, {"protocol", {{"major", 1}}}});
    check(Json{{"type", "hello"}, {"protocol", nullptr}});
    check(Json{{"type", "hello"}, {"protocol", Json::array()}});
    check(Json{{"type", "hello"}, {"protocol", Json::object()}});
}

TEST_F(JobserverIntegration, MalformedCommandIsRejectedWithoutWaitingForBusyResources) {
    auto owner{jobserver::Client::acquire({
        .metadata = {.name = "validation blocker", .kind = "test", .worktree = {}},
        .resources = {{.name = "machine", .mode = jobserver::ClaimMode::exclusive}},
    })};
    ASSERT_TRUE(owner);
    auto request = raw_submit_message("invalid waiting command", {"exit", "0"});
    request["resources"] = Json::array({{{"name", "machine"}, {"mode", "shared"}}});
    request["command"]["arguments"] = false;
    auto const pipe{connect_and_handshake_raw_pipe()};
    ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
    ASSERT_TRUE(jobserver::transport::write_message(pipe, request.dump(), 1s));
    auto const response{jobserver::transport::read_message(pipe, 1s)};
    CloseHandle(pipe);
    ASSERT_TRUE(response);
    auto const json = Json::parse(*response);
    EXPECT_EQ(json.value("type", ""), "error");
    EXPECT_EQ(json.value("code", ""), "invalid_request");
    auto const status{jobserver::Client::status()};
    ASSERT_TRUE(status);
    auto const state = Json::parse(*status);
    ASSERT_EQ(state["jobs"].size(), 1U);
    EXPECT_EQ(state["jobs"][0]["id"], owner->id());
    EXPECT_EQ(state["jobs"][0]["state"], "RUNNING");
    EXPECT_TRUE(owner->release());
}

TEST_F(JobserverIntegration, ValidOptionalFieldsAndUnknownExtensionsRemainCompatible) {
    auto const pipe{connect_and_handshake_raw_pipe()};
    ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
    auto const request = Json{
        {"type", "submit"},
        {"command", {{"executable", JOBSERVER_TEST_HELPER_PATH}, {"arguments", {"exit", "0", ""}}}},
        {"future_extension", Json::object()}};
    ASSERT_TRUE(jobserver::transport::write_message(pipe, request.dump(), 1s));
    auto const response{jobserver::transport::read_message(pipe, 1s)};
    CloseHandle(pipe);
    ASSERT_TRUE(response);
    auto const json = Json::parse(*response);
    EXPECT_EQ(json.value("type", ""), "completed");
    EXPECT_EQ(json.value("exit_code", 1), 0);
    EXPECT_TRUE(jobserver::Client::ping());
}

TEST_F(JobserverIntegration, MalformedLeaseReleaseRejectsFieldsAndStillReleasesOwnership) {
    for (auto const& release : std::vector<Json>{nullptr,
                                                 Json::array(),
                                                 Json{{"type", "release"}},
                                                 Json{{"type", "release"}, {"id", false}},
                                                 Json{{"type", "release"}, {"id", "wrong-job"}},
                                                 Json{{"type", false}, {"id", "wrong-job"}}}) {
        SCOPED_TRACE(release.dump());
        auto const pipe{connect_and_handshake_raw_pipe()};
        ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
        auto const request = Json{
            {"type", "acquire"},
            {"resources", Json::array({{{"name", "malformed-release"}, {"mode", "exclusive"}}})}};
        ASSERT_TRUE(jobserver::transport::write_message(pipe, request.dump(), 1s));
        auto const granted{jobserver::transport::read_message(pipe, 1s)};
        ASSERT_TRUE(granted);
        EXPECT_EQ(Json::parse(*granted).value("type", ""), "granted");
        ASSERT_TRUE(jobserver::transport::write_message(pipe, release.dump(), 1s));
        auto const response{jobserver::transport::read_message(pipe, 1s)};
        CloseHandle(pipe);
        ASSERT_TRUE(response);
        auto const json = Json::parse(*response);
        EXPECT_EQ(json.value("type", ""), "error");
        EXPECT_EQ(json.value("code", ""), "invalid_request");
        auto const status{jobserver::Client::status()};
        ASSERT_TRUE(status);
        auto const state = Json::parse(*status);
        EXPECT_EQ(state["daemon"]["scheduler_entries"], 0U);
        EXPECT_EQ(state["daemon"]["leases"], 0U);
        for (auto const& resource : state["resources"]) {
            EXPECT_FALSE(resource.value("exclusive", false));
        }
        EXPECT_TRUE(jobserver::Client::ping());
    }
}

TEST_F(JobserverIntegration, TruncatedAndOversizedFramesDoNotStopDaemon) {
    auto send_and_close = [](std::span<std::byte const> const bytes) {
        auto const pipe{connect_sync_raw_pipe()};
        EXPECT_NE(pipe, INVALID_HANDLE_VALUE);
        if (pipe != INVALID_HANDLE_VALUE) {
            EXPECT_TRUE(write_raw(pipe, bytes));
            CloseHandle(pipe);
        }
    };

    std::array<std::byte, 2> const partial_header{std::byte{8}, std::byte{0}};
    send_and_close(partial_header);

    std::array<std::byte, 6> const truncated_payload{
        std::byte{8}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{'{'}, std::byte{'"'}};
    send_and_close(truncated_payload);

    auto const oversized{jobserver::protocol::maximum_payload_size + 1U};
    std::array<std::byte, 4> const oversized_header{
        std::byte{oversized & 0xffU},
        std::byte{(oversized >> 8U) & 0xffU},
        std::byte{(oversized >> 16U) & 0xffU},
        std::byte{(oversized >> 24U) & 0xffU},
    };
    send_and_close(oversized_header);

    EXPECT_TRUE(jobserver::Client::ping().has_value());
}

TEST_F(JobserverIntegration, SlowHandshakeFloodIsBoundedAndDrains) {
    stop_daemon();
    ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_IO_TIMEOUT_MS", L"2000"), 0);
    start_daemon();

    std::vector<HANDLE> clients;
    clients.reserve(80);
    for (auto index{0}; index != 80; ++index) {
        auto const pipe{connect_sync_raw_pipe()};
        ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
        clients.push_back(pipe);
    }
    for (auto const pipe : clients) {
        CloseHandle(pipe);
    }

    Json daemon;
    for (auto attempt{0}; attempt != 300; ++attempt) {
        auto status{jobserver::Client::status()};
        if (status) {
            auto const json = Json::parse(*status);
            daemon = json.value("daemon", Json::object());
            if (daemon.value("active_handlers", 0U) == 1U) {
                break;
            }
        }
        std::this_thread::sleep_for(10ms);
    }
    ASSERT_TRUE(daemon.is_object());
    EXPECT_EQ(daemon.value("handler_capacity", 0U), 64U);
    EXPECT_GE(daemon.value("rejected_clients", 0ULL), 1ULL);
    EXPECT_EQ(daemon.value("active_handlers", 0U), 1U);
    EXPECT_TRUE(jobserver::Client::ping().has_value());

    stop_daemon();
    ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_IO_TIMEOUT_MS", L"250"), 0);
    start_daemon();
}

TEST_F(JobserverIntegration, ControlEndpointSurvivesSaturatedJobHandlers) {
    stop_daemon();
    TestIoTimeout const production_timeout{""};
    start_daemon();

    TestPipes clients;
    for (auto index{0}; index != 64; ++index) {
        auto const pipe{connect_and_handshake_raw_pipe()};
        if (pipe == INVALID_HANDLE_VALUE) {
            FAIL() << "Could not fill job-handler pool";
        }
        clients.handles.push_back(pipe);
        ASSERT_TRUE(jobserver::transport::write_message(pipe, Json{{"type", "ping"}}.dump(), 1s));
        ASSERT_TRUE(jobserver::transport::read_message(pipe, 1s));
    }

    auto const start{std::chrono::steady_clock::now()};
    auto status{jobserver::Client::status()};
    EXPECT_TRUE(status.has_value());
    if (status) {
        auto const daemon = Json::parse(*status).at("daemon");
        EXPECT_EQ(daemon.value("job_handlers", 0U), 64U);
        EXPECT_EQ(daemon.value("control_handler_capacity", 0U), 8U);
    }
    EXPECT_TRUE(jobserver::Client::ping());
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
    EXPECT_TRUE(jobserver::Client::shutdown());
    EXPECT_TRUE(wait_for_exit(daemon_, 1s));
}

TEST_F(JobserverIntegration, KillInterruptsPendingOutputWithProductionWriteDeadline) {
    stop_daemon();
    TestIoTimeout const production_timeout{""};
    TestBarrier barrier{"during_output_write_pending", 900};
    start_daemon();
    auto const pipe{connect_and_handshake_raw_pipe()};
    ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
    TestPipes const pipes{{pipe}};
    auto const message =
        raw_submit_message("blocked production output", {"large-output", "16777216"});
    ASSERT_TRUE(jobserver::transport::write_message(pipe, message.dump(), 1s));
    auto const pending{barrier.wait(3s)};
    auto const job{find_job("blocked production output")};
    EXPECT_TRUE(pending);
    EXPECT_TRUE(job.has_value());
    if (job) {
        EXPECT_TRUE(jobserver::Client::cancel(job->value("id", ""), true));
    }
    auto const start{std::chrono::steady_clock::now()};
    barrier.release();
    EXPECT_TRUE(wait_for_job_to_disappear("blocked production output", 1s));
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
    EXPECT_TRUE(jobserver::Client::shutdown());
    EXPECT_TRUE(wait_for_exit(daemon_, 1s));
}

TEST_F(JobserverIntegration, TimeoutInterruptsPendingOutputWithProductionWriteDeadline) {
    stop_daemon();
    TestIoTimeout const production_timeout{""};
    TestBarrier barrier{"during_output_write_pending", 901};
    start_daemon();
    auto const pipe{connect_and_handshake_raw_pipe()};
    ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
    TestPipes const pipes{{pipe}};
    auto message = raw_submit_message("timed out production output", {"large-output", "16777216"});
    message["timeout_ms"] = 500;
    ASSERT_TRUE(jobserver::transport::write_message(pipe, message.dump(), 1s));
    EXPECT_TRUE(barrier.wait(3s));
    barrier.release();
    EXPECT_TRUE(wait_for_job_to_disappear("timed out production output", 2s));
    auto history{jobserver::Client::status(true)};
    EXPECT_TRUE(history.has_value());
    if (history) {
        auto const jobs = Json::parse(*history).at("jobs");
        EXPECT_TRUE(std::ranges::any_of(jobs, [](Json const& job) {
            return job.value("name", "") == "timed out production output" &&
                   job.value("state", "") == "TIMED_OUT";
        }));
    }
    EXPECT_TRUE(jobserver::Client::shutdown());
    EXPECT_TRUE(wait_for_exit(daemon_, 1s));
}

TEST_F(JobserverIntegration, ProductionIdleShutdownInterruptsIncompleteClientsOnBothEndpoints) {
    stop_daemon();
    TestIoTimeout const production_timeout{""};
    start_daemon();
    TestPipes clients;
    for (auto const control : {false, true}) {
        auto const pipe{connect_raw_pipe(control)};
        ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
        clients.handles.push_back(pipe);
    }
    auto const start{std::chrono::steady_clock::now()};
    EXPECT_TRUE(jobserver::Client::shutdown());
    EXPECT_TRUE(wait_for_exit(daemon_, 1s));
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
}

TEST_F(JobserverIntegration, IncompleteLeaseFrameExpiresButIdleLeaseDoesNot) {
    for (auto const partial_payload : {false, true}) {
        auto const pipe{connect_sync_raw_pipe()};
        ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
        auto const hello = Json{{"type", "hello"}, {"protocol", {{"major", 1}, {"minor", 1}}}};
        ASSERT_TRUE(jobserver::transport::write_message(pipe, hello.dump()));
        ASSERT_TRUE(jobserver::transport::read_message(pipe));
        auto const request =
            Json{{"type", "acquire"},
                 {"metadata", {{"name", "partial lease"}}},
                 {"resources",
                  Json::array({{{"name", "integration-resource"}, {"mode", "exclusive"}}})}};
        ASSERT_TRUE(jobserver::transport::write_message(pipe, request.dump()));
        ASSERT_TRUE(jobserver::transport::read_message(pipe));
        std::this_thread::sleep_for(350ms);
        EXPECT_TRUE(find_job("partial lease"));
        std::array<std::byte, 5> const prefix{
            std::byte{20}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{'{'}};
        EXPECT_TRUE(write_raw(pipe, std::span{prefix}.first(partial_payload ? 5 : 1)));
        EXPECT_TRUE(wait_for_job_to_disappear("partial lease", 1s));
        CloseHandle(pipe);
        auto next{jobserver::Client::acquire(
            test_request("after partial frame", jobserver::ClaimMode::exclusive))};
        ASSERT_TRUE(next);
        EXPECT_TRUE(next->release());
    }
}

TEST_F(JobserverIntegration, ControlEndpointRejectsJobAdmission) {
    auto const pipe{connect_and_handshake_raw_pipe(true)};
    ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
    auto const request = raw_submit_message("not a control job", {"exit", "0"});
    EXPECT_TRUE(jobserver::transport::write_message(pipe, request.dump(), 1s));
    auto const response{jobserver::transport::read_message(pipe, 1s)};
    CloseHandle(pipe);
    ASSERT_TRUE(response);
    EXPECT_EQ(Json::parse(*response).value("code", ""), "control_only");
    EXPECT_TRUE(wait_for_job_to_disappear("not a control job", 1s));
}

TEST_F(JobserverIntegration, ControlClientFallsBackToLegacyMainEndpoint) {
    stop_daemon();
    auto const pipe{
        CreateNamedPipeW(jobserver::transport::pipe_name().c_str(),
                         PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                         PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
                         1,
                         65536,
                         65536,
                         0,
                         nullptr)};
    ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
    auto const event{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    ASSERT_NE(event, nullptr);
    TestPipes const handles{{pipe, event}};
    OVERLAPPED connected{};
    connected.hEvent = event;
    auto const immediate{ConnectNamedPipe(pipe, &connected) != FALSE};
    ASSERT_TRUE(immediate || GetLastError() == ERROR_IO_PENDING);
    auto client{std::async(std::launch::async, [] { return jobserver::Client::ping(); })};
    if (!immediate && WaitForSingleObject(event, 1000) != WAIT_OBJECT_0) {
        CancelIoEx(pipe, &connected);
        WaitForSingleObject(event, INFINITE);
        FAIL() << "Client did not connect to legacy endpoint";
    }
    ASSERT_TRUE(jobserver::transport::read_message(pipe, 1s));
    ASSERT_TRUE(jobserver::transport::write_message(
        pipe, Json{{"type", "hello_ack"}, {"protocol", {{"major", 1}, {"minor", 1}}}}.dump(), 1s));
    auto const request{jobserver::transport::read_message(pipe, 1s)};
    ASSERT_TRUE(request);
    EXPECT_EQ(Json::parse(*request).value("type", ""), "ping");
    ASSERT_TRUE(jobserver::transport::write_message(pipe, Json{{"type", "pong"}}.dump(), 1s));
    EXPECT_TRUE(client.get());
}

TEST_F(JobserverIntegration, OversizedHistoryReturnsStructuredErrorInsteadOfReadTimeout) {
    stop_daemon();
    {
        std::ofstream history{data_path_ / "history.jsonl", std::ios::trunc};
        for (auto index{0}; index != 600; ++index) {
            history << Json{{"id", "oversized-" + std::to_string(index)},
                            {"name", std::string(2000, 'x')},
                            {"state", "SUCCEEDED"}}
                           .dump()
                    << '\n';
        }
    }
    start_daemon();
    auto history{jobserver::Client::status(true)};
    ASSERT_FALSE(history.has_value());
    EXPECT_EQ(history.error().code, "payload_too_large");
    EXPECT_TRUE(jobserver::Client::status(false));
    EXPECT_TRUE(jobserver::Client::ping());
    stop_daemon();
    std::filesystem::remove(data_path_ / "history.jsonl");
}

TEST_F(JobserverIntegration, LoadsOnlyNewestValidHistoryEntries) {
    stop_daemon();
    auto const history_path{data_path_ / "history.jsonl"};
    {
        std::ofstream history{history_path, std::ios::trunc};
        history << "corrupt history line\n";
        for (auto index{0}; index != 1005; ++index) {
            history << Json{{"id", "history-" + std::to_string(index)},
                            {"name", "history"},
                            {"state", "SUCCEEDED"}}
                           .dump()
                    << '\n';
        }
    }
    start_daemon();
    auto status{jobserver::Client::status(true)};
    ASSERT_TRUE(status.has_value());
    auto const jobs = Json::parse(*status).value("jobs", Json::array());
    ASSERT_EQ(jobs.size(), 1000U);
    EXPECT_EQ(jobs.front().value("id", ""), "history-5");
    EXPECT_EQ(jobs.back().value("id", ""), "history-1004");
    EXPECT_FALSE(jobs.front().contains("submit_directory"));

    auto const submit_directory{jobserver::path_to_utf8(std::filesystem::current_path())};

    auto const run_result{jobserver::Client::run(
        {
            .metadata = {.name = "history rotation", .kind = "test", .worktree = data_path_},
            .command = {.executable = JOBSERVER_TEST_HELPER_PATH,
                        .arguments = {"exit", "0"},
                        .working_directory =
                            std::filesystem::path{JOBSERVER_TEST_HELPER_PATH}.parent_path(),
                        .environment = {}},
            .resources = {},
            .timeout = std::nullopt,
            .suspect_after = std::nullopt,
            .disconnect_policy = jobserver::DisconnectPolicy::cancel,
        },
        [](std::string const&, std::string const&) {})};
    ASSERT_TRUE(run_result.has_value());
    std::ifstream rotated_history{history_path};
    auto valid_lines{0};
    auto persisted_provenance{false};
    std::string line;
    while (std::getline(rotated_history, line)) {
        auto const entry = Json::parse(line, nullptr, false);
        if (entry.is_object()) {
            ++valid_lines;
            persisted_provenance =
                persisted_provenance ||
                (entry.value("name", "") == "history rotation" &&
                 entry.value("worktree", "") == jobserver::path_to_utf8(data_path_) &&
                 entry.value("submit_directory", "") == submit_directory);
        }
    }
    EXPECT_EQ(valid_lines, 1000);
    EXPECT_TRUE(persisted_provenance);

    stop_daemon();
    start_daemon();
    auto const reloaded{jobserver::Client::status(true)};
    ASSERT_TRUE(reloaded.has_value());
    auto const reloaded_jobs = Json::parse(*reloaded).value("jobs", Json::array());
    auto const restored{std::ranges::find_if(reloaded_jobs, [](Json const& job) {
        return job.value("name", "") == "history rotation";
    })};
    ASSERT_NE(restored, reloaded_jobs.end());
    EXPECT_EQ(restored->value("submit_directory", ""), submit_directory);
}

TEST_F(JobserverIntegration, RotatesAndWritesDaemonDiagnosticLog) {
    stop_daemon();
    auto const log_path{data_path_ / "jobserverd.log"};
    auto const previous_path{data_path_ / "jobserverd.previous.log"};
    {
        std::ofstream oversized{log_path, std::ios::binary | std::ios::trunc};
        oversized << std::string(1024U * 1024U + 1U, 'x');
    }

    start_daemon();

    ASSERT_TRUE(std::filesystem::exists(previous_path));
    EXPECT_GT(std::filesystem::file_size(previous_path), 1024U * 1024U);
    std::ifstream current{log_path};
    auto const contents{std::string{std::istreambuf_iterator<char>{current}, {}}};
    EXPECT_NE(contents.find("Jobserver daemon starting"), std::string::npos) << contents;
}

TEST_F(JobserverIntegration, RefusesShutdownWhileLeaseIsActive) {
    auto lease{jobserver::Client::acquire(
        test_request("shutdown blocker", jobserver::ClaimMode::exclusive))};
    ASSERT_TRUE(lease.has_value());
    auto shutdown{jobserver::Client::shutdown()};
    ASSERT_FALSE(shutdown.has_value());
    EXPECT_EQ(shutdown.error().code, "shutdown_refused");
    ASSERT_TRUE(jobserver::Client::status().has_value());
    ASSERT_TRUE(lease->release().has_value());
}

TEST_F(JobserverIntegration, DaemonCrashKillsDescendantProcessTree) {
    auto const marker{data_path_ / "daemon-crash-marker.txt"};
    std::filesystem::remove(marker);
    auto run_future{std::async(std::launch::async, [&] {
        return jobserver::Client::run(
            {
                .metadata = {.name = "daemon crash tree", .kind = "test", .worktree = data_path_},
                .command = {.executable = JOBSERVER_TEST_HELPER_PATH,
                            .arguments = {"spawn-marker", marker.string(), "1500"},
                            .working_directory =
                                std::filesystem::path{JOBSERVER_TEST_HELPER_PATH}.parent_path(),
                            .environment = {}},
                .resources = {},
                .timeout = std::nullopt,
                .suspect_after = std::nullopt,
                .disconnect_policy = jobserver::DisconnectPolicy::continue_job,
            },
            [](std::string const&, std::string const&) {});
    })};
    ASSERT_TRUE(find_job("daemon crash tree").has_value());
    ASSERT_TRUE(TerminateProcess(daemon_.process, 77));
    ASSERT_TRUE(wait_for_exit(daemon_, 2s).has_value());
    close(daemon_);
    ASSERT_EQ(run_future.wait_for(2s), std::future_status::ready);
    EXPECT_FALSE(run_future.get().has_value());
    std::this_thread::sleep_for(1700ms);
    EXPECT_FALSE(std::filesystem::exists(marker));
}

TEST_F(JobserverIntegration, DaemonCrashDrainsMixedWorkloadAndRestartsCleanly) {
    auto const helper_processes_before{process_ids_named(L"jobserver-test-helper.exe")};
    auto const first_marker{data_path_ / "mixed-crash-first.txt"};
    auto const second_marker{data_path_ / "mixed-crash-second.txt"};
    auto const queued_marker{data_path_ / "mixed-crash-queued.txt"};
    std::filesystem::remove(first_marker);
    std::filesystem::remove(second_marker);
    std::filesystem::remove(queued_marker);

    auto run = [](jobserver::SubmitRequest request) {
        return std::async(std::launch::async, [request = std::move(request)] {
            return jobserver::Client::run(request, [](std::string const&, std::string const&) {});
        });
    };
    auto first{run(
        submit_request("mixed crash first",
                       {{.name = "mixed-crash-resource", .mode = jobserver::ClaimMode::exclusive}},
                       {"spawn-marker", first_marker.string(), "1500"}))};
    auto const first_job{find_job_in_state("mixed crash first", "RUNNING")};
    ASSERT_TRUE(first_job.has_value());

    auto second{run(submit_request(
        "mixed crash second",
        {{.name = "mixed-crash-independent", .mode = jobserver::ClaimMode::exclusive}},
        {"spawn-marker", second_marker.string(), "1500"}))};
    auto const second_job{find_job_in_state("mixed crash second", "RUNNING")};
    ASSERT_TRUE(second_job.has_value());

    auto queued{run(
        submit_request("mixed crash queued",
                       {{.name = "mixed-crash-resource", .mode = jobserver::ClaimMode::exclusive}},
                       {"marker-after", queued_marker.string(), "10"}))};
    auto const queued_job{find_job_in_state("mixed crash queued", "QUEUED")};
    ASSERT_TRUE(queued_job.has_value());

    ASSERT_TRUE(TerminateProcess(daemon_.process, 78));
    ASSERT_TRUE(wait_for_exit(daemon_, 2s).has_value());
    close(daemon_);
    ASSERT_EQ(first.wait_for(2s), std::future_status::ready);
    ASSERT_EQ(second.wait_for(2s), std::future_status::ready);
    ASSERT_EQ(queued.wait_for(2s), std::future_status::ready);
    EXPECT_FALSE(first.get().has_value());
    EXPECT_FALSE(second.get().has_value());
    EXPECT_FALSE(queued.get().has_value());
    std::this_thread::sleep_for(1700ms);
    EXPECT_FALSE(std::filesystem::exists(first_marker));
    EXPECT_FALSE(std::filesystem::exists(second_marker));
    EXPECT_FALSE(std::filesystem::exists(queued_marker));
    EXPECT_EQ(process_ids_named(L"jobserver-test-helper.exe"), helper_processes_before);

    start_daemon();
    auto status{jobserver::Client::status()};
    ASSERT_TRUE(status.has_value());
    EXPECT_TRUE(Json::parse(*status).value("jobs", Json::array()).empty());
    auto lease{jobserver::Client::acquire(
        test_request("post-crash lease", jobserver::ClaimMode::exclusive))};
    ASSERT_TRUE(lease.has_value());
    EXPECT_TRUE(lease->release().has_value());
    auto run_result{jobserver::Client::run(submit_request("post-crash command", {}, {"exit", "0"}),
                                           [](std::string const&, std::string const&) {})};
    ASSERT_TRUE(run_result.has_value());
    EXPECT_EQ(*run_result, 0);
}

TEST_F(JobserverIntegration, MixedClientSoakDrainsWithoutLeakingHandles) {
    DWORD handles_before{};
    ASSERT_TRUE(GetProcessHandleCount(daemon_.process, &handles_before));

    constexpr auto job_count{48};
    std::vector<std::future<std::expected<int, jobserver::Error>>> futures;
    futures.reserve(job_count);
    std::vector<int> expected_exit_codes;
    expected_exit_codes.reserve(job_count);
    for (auto index{0}; index != job_count; ++index) {
        std::vector<jobserver::ResourceClaim> resources{
            {.name = "cpu",
             .mode = jobserver::ClaimMode::counted,
             .units = static_cast<std::uint32_t>(index % 4 + 1)},
            {.name = "machine",
             .mode =
                 index % 12 == 0 ? jobserver::ClaimMode::exclusive : jobserver::ClaimMode::shared},
        };
        if (index % 7 == 0) {
            resources.push_back({.name = "soak-extra-" + std::to_string(index % 3),
                                 .mode = jobserver::ClaimMode::exclusive});
        }

        auto arguments = std::vector<std::string>{"sleep", std::to_string(index % 5 + 2)};
        auto timeout = std::optional<std::chrono::milliseconds>{};
        auto expected{0};
        if (index % 13 == 0) {
            arguments = {"sleep", "100"};
            timeout = 20ms;
            expected = 124;
        } else if (index % 17 == 0) {
            arguments = {"crash"};
            expected = -1;
        } else if (index % 11 == 0) {
            arguments = {"exit", "7"};
            expected = 7;
        } else if (index % 5 == 0) {
            arguments = {"output", "2"};
        }
        expected_exit_codes.push_back(expected);
        auto request{submit_request(
            "soak-" + std::to_string(index), std::move(resources), std::move(arguments), timeout)};
        futures.push_back(std::async(std::launch::async, [request = std::move(request)] {
            return jobserver::Client::run(request, [](std::string const&, std::string const&) {});
        }));
    }

    for (auto index{0}; index != job_count; ++index) {
        ASSERT_EQ(futures[static_cast<std::size_t>(index)].wait_for(10s),
                  std::future_status::ready);
        auto result{futures[static_cast<std::size_t>(index)].get()};
        ASSERT_TRUE(result.has_value()) << result.error().message;
        auto const expected{expected_exit_codes[static_cast<std::size_t>(index)]};
        if (expected < 0) {
            EXPECT_NE(*result, 0);
        } else {
            std::string diagnostics;
            if (*result != expected) {
                std::ifstream log{data_path_ / "jobserverd.log"};
                diagnostics.assign(std::istreambuf_iterator<char>{log},
                                   std::istreambuf_iterator<char>{});
                if (diagnostics.size() > 4096) {
                    diagnostics.erase(0, diagnostics.size() - 4096);
                }
            }
            EXPECT_EQ(*result, expected) << "soak job index " << index << '\n' << diagnostics;
        }
    }

    std::vector<ChildProcess> crashing_clients;
    for (auto index{0}; index != 4; ++index) {
        auto child{launch(JOBSERVER_TEST_CLIENT_PATH, L"lease-crash")};
        ASSERT_NE(child.process, nullptr);
        crashing_clients.push_back(child);
    }
    for (auto& child : crashing_clients) {
        auto const exit_code{wait_for_exit(child, 5s)};
        ASSERT_TRUE(exit_code.has_value());
        EXPECT_EQ(*exit_code, 99U);
        close(child);
    }
    auto crash_resource_lease{jobserver::Client::acquire({
        .metadata = {.name = "post-soak crash lease", .kind = "test", .worktree = {}},
        .resources = {{.name = "crash-resource", .mode = jobserver::ClaimMode::exclusive}},
    })};
    ASSERT_TRUE(crash_resource_lease.has_value());
    ASSERT_TRUE(crash_resource_lease->release().has_value());

    auto cancel_future{std::async(std::launch::async, [&] {
        return jobserver::Client::run(submit_request("soak cancellation", {}, {"sleep", "60000"}),
                                      [](std::string const&, std::string const&) {});
    })};
    auto const cancellation_job{find_job("soak cancellation")};
    ASSERT_TRUE(cancellation_job.has_value());
    ASSERT_TRUE(jobserver::Client::cancel(cancellation_job->value("id", ""), false).has_value());
    ASSERT_EQ(cancel_future.wait_for(3s), std::future_status::ready);
    auto cancel_result{cancel_future.get()};
    ASSERT_TRUE(cancel_result.has_value());
    EXPECT_EQ(*cancel_result, 130);

    for (auto attempt{0}; attempt != 50; ++attempt) {
        auto status{jobserver::Client::status()};
        ASSERT_TRUE(status.has_value());
        if (Json::parse(*status).value("jobs", Json::array()).empty()) {
            break;
        }
        std::this_thread::sleep_for(20ms);
    }
    auto final_status{jobserver::Client::status()};
    ASSERT_TRUE(final_status.has_value());
    EXPECT_TRUE(Json::parse(*final_status).value("jobs", Json::array()).empty());

    DWORD handles_after{};
    ASSERT_TRUE(GetProcessHandleCount(daemon_.process, &handles_after));
    EXPECT_LE(handles_after, handles_before + 16U);
}

TEST_F(JobserverIntegration, SlowAndAbandonedOutputClientsDoNotBlockControlPlane) {
    std::mutex output_mutex;
    std::string output;
    output.reserve(1024U * 1024U);
    auto slow_future{std::async(std::launch::async, [&] {
        return jobserver::Client::run(
            submit_request("slow output reader", {}, {"large-output", "1048576"}),
            [&](std::string const&, std::string const& text) {
                std::this_thread::sleep_for(1ms);
                std::scoped_lock const lock{output_mutex};
                output += text;
            });
    })};
    auto const slow_job{find_job("slow output reader")};
    ASSERT_TRUE(slow_job.has_value());
    auto status_start{std::chrono::steady_clock::now()};
    EXPECT_TRUE(jobserver::Client::status().has_value());
    EXPECT_LT(std::chrono::steady_clock::now() - status_start, 1s);
    auto slow_wait{slow_future.wait_for(10s)};
    if (slow_wait != std::future_status::ready) {
        static_cast<void>(jobserver::Client::cancel(slow_job->value("id", ""), true));
        slow_wait = slow_future.wait_for(3s);
    }
    ASSERT_EQ(slow_wait, std::future_status::ready);
    auto slow_result{slow_future.get()};
    ASSERT_TRUE(slow_result.has_value());
    EXPECT_EQ(*slow_result, 0);
    EXPECT_EQ(output, std::string(1024U * 1024U, 'x'));

    auto const id{slow_job->value("id", "")};
    std::ifstream log{data_path_ / "logs" / (id + ".stdout.log"), std::ios::binary};
    std::string const logged{std::istreambuf_iterator<char>{log}, std::istreambuf_iterator<char>{}};
    EXPECT_EQ(logged, output);

    auto pipe{connect_and_handshake_raw_pipe()};
    ASSERT_NE(pipe, INVALID_HANDLE_VALUE);
    auto const message =
        raw_submit_message("abandoned output reader", {"large-output", "16777216"});
    ASSERT_TRUE(message.is_object()) << message.dump();
    auto const serialized_message{message.dump()};
    ASSERT_TRUE(jobserver::transport::write_message(pipe, serialized_message).has_value());
    auto const first_output{jobserver::transport::read_message(pipe)};
    if (!first_output) {
        CloseHandle(pipe);
        FAIL() << "Abandoned output client did not receive initial output";
    }
    EXPECT_EQ(Json::parse(*first_output).value("type", ""), "output") << *first_output;
    auto const abandoned_job{find_job("abandoned output reader")};
    if (!abandoned_job) {
        CloseHandle(pipe);
        FAIL() << "Abandoned output job was not observable";
    }
    status_start = std::chrono::steady_clock::now();
    EXPECT_TRUE(jobserver::Client::status().has_value());
    EXPECT_LT(std::chrono::steady_clock::now() - status_start, 1s);
    ASSERT_TRUE(jobserver::Client::cancel(abandoned_job->value("id", ""), true).has_value());

    auto drained{false};
    for (auto attempt{0}; attempt != 100; ++attempt) {
        auto status{jobserver::Client::status()};
        ASSERT_TRUE(status.has_value());
        auto const jobs = Json::parse(*status).value("jobs", Json::array());
        if (std::ranges::none_of(jobs, [](Json const& job) {
                return job.value("name", "") == "abandoned output reader";
            })) {
            drained = true;
            break;
        }
        std::this_thread::sleep_for(20ms);
    }
    EXPECT_TRUE(drained) << "Abandoned output job did not drain";

    auto const shutdown_start{std::chrono::steady_clock::now()};
    auto shutdown{jobserver::Client::shutdown()};
    EXPECT_TRUE(shutdown.has_value()) << shutdown.error().message;
    EXPECT_TRUE(wait_for_exit(daemon_, 2s).has_value());
    EXPECT_LT(std::chrono::steady_clock::now() - shutdown_start, 1500ms);
    CloseHandle(pipe);
    close(daemon_);
}

TEST_F(JobserverIntegration, DisconnectPolicyControlsWhetherChildOutlivesClient) {
    auto const attached_marker{data_path_ / "attached-client-marker.txt"};
    auto const detached_marker{data_path_ / "detached-client-marker.txt"};
    std::filesystem::remove(attached_marker);
    std::filesystem::remove(detached_marker);

    auto attached{
        launch(JOBSERVER_TEST_CLIENT_PATH, L"attached-run \"" + attached_marker.wstring() + L"\"")};
    ASSERT_NE(attached.process, nullptr);
    ASSERT_TRUE(find_job_in_state("attached client job", "RUNNING").has_value());
    ASSERT_TRUE(TerminateProcess(attached.process, 99));
    ASSERT_TRUE(wait_for_exit(attached, 2s).has_value());
    close(attached);
    ASSERT_TRUE(wait_for_job_to_disappear("attached client job", 2s));

    auto detached{
        launch(JOBSERVER_TEST_CLIENT_PATH, L"detached-run \"" + detached_marker.wstring() + L"\"")};
    ASSERT_NE(detached.process, nullptr);
    ASSERT_TRUE(find_job_in_state("detached client job", "RUNNING").has_value());
    ASSERT_TRUE(TerminateProcess(detached.process, 99));
    ASSERT_TRUE(wait_for_exit(detached, 2s).has_value());
    close(detached);

    EXPECT_FALSE(std::filesystem::exists(attached_marker));
    EXPECT_TRUE(wait_for_file(detached_marker, 3s));
}
}
