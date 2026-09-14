#include "jobserver/client.hpp"
#include "jobserver/protocol.hpp"
#include "jobserver/transport.hpp"

#include <Windows.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
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
                                    0,
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

auto test_request(std::string name, jobserver::ClaimMode const mode) -> jobserver::AcquireRequest {
    return {
        .metadata = {.name = std::move(name), .kind = "test", .worktree = {}},
        .resources = {{.name = "integration-resource", .mode = mode}},
    };
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
        ASSERT_EQ(_wputenv_s(L"NUKETHEBEES_JOBSERVER_JOB", L""), 0);
    }

    static void TearDownTestSuite() {
        std::filesystem::remove_all(data_path_);
        static_cast<void>(_wputenv_s(L"NUKETHEBEES_JOBSERVER_TEST_PIPE", L""));
        static_cast<void>(_putenv_s("NUKETHEBEES_JOBSERVER_TEST_DATA", ""));
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
                      L"lease-hold worktree-one cross-worktree-resource 800",
                      first_worktree)};
    ASSERT_NE(first.process, nullptr);
    auto const first_job{find_job("worktree-one")};
    ASSERT_TRUE(first_job.has_value());
    EXPECT_EQ(first_job->value("worktree", ""), first_worktree.string());

    auto second{launch(JOBSERVER_TEST_CLIENT_PATH,
                       L"lease-hold worktree-two cross-worktree-resource 50",
                       second_worktree)};
    ASSERT_NE(second.process, nullptr);
    auto const second_job{find_job("worktree-two")};
    ASSERT_TRUE(second_job.has_value());
    EXPECT_EQ(second_job->value("state", ""), "QUEUED");
    EXPECT_EQ(second_job->value("worktree", ""), second_worktree.string());

    auto const first_exit{wait_for_exit(first, 3s)};
    auto const second_exit{wait_for_exit(second, 3s)};
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
