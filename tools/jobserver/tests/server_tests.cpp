#include "jobserver/client.hpp"

#include <Windows.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <thread>

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

auto launch(std::filesystem::path const& executable, std::wstring arguments = {}) -> ChildProcess {
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
                        nullptr,
                        &startup,
                        &process)) {
        return {};
    }
    return {.process = process.hProcess, .thread = process.hThread};
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
}
