#include "jobserver/client.hpp"
#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"

#include <Windows.h>

#include <tlhelp32.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
using Json = nlohmann::json;

struct ChildProcess {
    HANDLE process{};
    HANDLE thread{};
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

auto connect_raw_pipe() -> HANDLE {
    for (auto attempt{0}; attempt != 50; ++attempt) {
        auto const pipe{CreateFileW(jobserver::transport::pipe_name().c_str(),
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

auto connect_and_handshake_raw_pipe() -> HANDLE {
    auto pipe{connect_raw_pipe()};
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

TEST_F(JobserverIntegration, ConflictingClientsAreGrantedInFifoOrder) {
    auto active{
        jobserver::Client::acquire(test_request("active shared", jobserver::ClaimMode::shared))};
    ASSERT_TRUE(active.has_value());
    auto exclusive_future{std::async(std::launch::async, [] {
        return jobserver::Client::acquire(
            test_request("waiting exclusive", jobserver::ClaimMode::exclusive));
    })};
    ASSERT_TRUE(find_job("waiting exclusive").has_value());
    auto later_future{std::async(std::launch::async, [] {
        return jobserver::Client::acquire(
            test_request("later shared", jobserver::ClaimMode::shared));
    })};
    ASSERT_TRUE(find_job("later shared").has_value());

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

    auto second{launch(JOBSERVER_TEST_CLIENT_PATH,
                       L"lease-hold worktree-two cross-worktree-resource 50",
                       second_worktree)};
    ASSERT_NE(second.process, nullptr);
    auto const second_job{find_job("worktree-two")};
    ASSERT_TRUE(second_job.has_value());
    EXPECT_EQ(second_job->value("state", ""), "QUEUED");
    EXPECT_EQ(second_job->value("worktree", ""), jobserver::path_to_utf8(second_worktree));

    auto const first_exit{wait_for_exit(first, 5s)};
    auto const second_exit{wait_for_exit(second, 5s)};
    ASSERT_TRUE(first_exit.has_value());
    ASSERT_TRUE(second_exit.has_value());
    EXPECT_EQ(*first_exit, 0U);
    EXPECT_EQ(*second_exit, 0U);
    close(first);
    close(second);
}

TEST_F(JobserverIntegration, NestedCommandInheritsInvokingBuildDirectory) {
    auto const build_directory{data_path_ / "nested-build-directory"};
    auto const worktree{data_path_ / "nested-worktree"};
    auto const marker{std::filesystem::path{"nested-marker.txt"}};
    std::filesystem::create_directories(build_directory);
    std::filesystem::create_directories(worktree);

    ASSERT_EQ(_putenv_s("NUKETHEBEES_JOBSERVER_JOB", "test-parent"), 0);
    auto child{launch(JOBSERVER_CLI_PATH,
                      L"run --name nested-working-directory --kind test --worktree " +
                          quote(worktree.wstring()) + L" -- " +
                          quote(std::filesystem::path{JOBSERVER_TEST_HELPER_PATH}.wstring()) +
                          L" marker-after " + marker.wstring() + L" 0",
                      build_directory)};
    ASSERT_EQ(_putenv_s("NUKETHEBEES_JOBSERVER_JOB", ""), 0);
    ASSERT_NE(child.process, nullptr);
    auto const exit_code{wait_for_exit(child, 3s)};
    ASSERT_TRUE(exit_code.has_value());
    EXPECT_EQ(*exit_code, 0U);
    EXPECT_TRUE(std::filesystem::exists(build_directory / marker));
    EXPECT_FALSE(std::filesystem::exists(worktree / marker));
    close(child);
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
    std::string line;
    while (std::getline(rotated_history, line)) {
        if (Json::parse(line, nullptr, false).is_object()) {
            ++valid_lines;
        }
    }
    EXPECT_EQ(valid_lines, 1000);
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
            EXPECT_EQ(*result, expected);
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
        return;
    }
    EXPECT_EQ(Json::parse(*first_output).value("type", ""), "output") << *first_output;
    auto const abandoned_job{find_job("abandoned output reader")};
    if (!abandoned_job) {
        CloseHandle(pipe);
        FAIL() << "Abandoned output job was not observable";
        return;
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
    ASSERT_TRUE(find_job("attached client job").has_value());
    ASSERT_TRUE(TerminateProcess(attached.process, 99));
    ASSERT_TRUE(wait_for_exit(attached, 2s).has_value());
    close(attached);

    auto detached{
        launch(JOBSERVER_TEST_CLIENT_PATH, L"detached-run \"" + detached_marker.wstring() + L"\"")};
    ASSERT_NE(detached.process, nullptr);
    ASSERT_TRUE(find_job("detached client job").has_value());
    ASSERT_TRUE(TerminateProcess(detached.process, 99));
    ASSERT_TRUE(wait_for_exit(detached, 2s).has_value());
    close(detached);

    std::this_thread::sleep_for(1s);
    EXPECT_FALSE(std::filesystem::exists(attached_marker));
    EXPECT_TRUE(std::filesystem::exists(detached_marker));
}
}
