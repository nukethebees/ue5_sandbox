#include "supervisor.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <mutex>
#include <string>

namespace {
auto helper_command(std::vector<std::string> arguments) -> jobserver::Command {
    return jobserver::Command{
        .executable = JOBSERVER_TEST_HELPER_PATH,
        .arguments = std::move(arguments),
        .working_directory = std::filesystem::path{JOBSERVER_TEST_HELPER_PATH}.parent_path(),
        .environment = {},
    };
}
}

TEST(JobserverSupervisor, PropagatesExitCodeAndOutput) {
    jobserver::Supervisor supervisor;
    std::mutex output_mutex;
    std::string stdout_text;
    std::string stderr_text;
    auto const result{supervisor.run(
        helper_command({"output", "2"}),
        std::nullopt,
        std::nullopt,
        [&](std::string const& stream, std::string const& text) {
            std::scoped_lock const lock{output_mutex};
            auto& output{stream == "stderr" ? stderr_text : stdout_text};
            output += text;
        },
        [](jobserver::JobHealth, std::string) {},
        [] { return true; })};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->exit_code, 0);
    EXPECT_FALSE(result->killed);
    EXPECT_NE(stdout_text.find("stdout 0"), std::string::npos) << stdout_text;
    EXPECT_NE(stderr_text.find("stderr 1"), std::string::npos) << stderr_text;
}

TEST(JobserverSupervisor, TimeoutTerminatesProcessTree) {
    jobserver::Supervisor supervisor;
    auto const result{supervisor.run(
        helper_command({"spawn"}),
        std::chrono::milliseconds{100},
        std::nullopt,
        [](std::string const&, std::string const&) {},
        [](jobserver::JobHealth, std::string) {},
        [] { return true; })};
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->timed_out);
    EXPECT_TRUE(result->killed);
    EXPECT_EQ(result->termination_exit_code, 137);
}

TEST(JobserverSupervisor, DisconnectTerminatesAttachedProcess) {
    jobserver::Supervisor supervisor;
    auto const result{supervisor.run(
        helper_command({"sleep", "60000"}),
        std::nullopt,
        std::nullopt,
        [](std::string const&, std::string const&) {},
        [](jobserver::JobHealth, std::string) {},
        [] { return false; })};
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->killed);
    EXPECT_EQ(result->termination_exit_code, 130);
}

TEST(JobserverSupervisor, ReportsSuspectedHangWithoutUsingItAsKillPolicy) {
    jobserver::Supervisor supervisor;
    bool suspected{};
    auto const result{supervisor.run(
        helper_command({"sleep", "300"}),
        std::nullopt,
        std::chrono::milliseconds{30},
        [](std::string const&, std::string const&) {},
        [&](jobserver::JobHealth const health, std::string) {
            suspected = suspected || health == jobserver::JobHealth::suspected_hang;
        },
        [] { return true; })};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->exit_code, 0);
    EXPECT_FALSE(result->killed);
    EXPECT_TRUE(suspected);
}

TEST(JobserverSupervisor, ReportsCrashedProcessExit) {
    jobserver::Supervisor supervisor;
    auto const result{supervisor.run(
        helper_command({"crash"}),
        std::nullopt,
        std::nullopt,
        [](std::string const&, std::string const&) {},
        [](jobserver::JobHealth, std::string) {},
        [] { return true; })};
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->exit_code, 0);
    EXPECT_FALSE(result->killed);
    EXPECT_FALSE(result->timed_out);
}

TEST(JobserverSupervisor, CancellationRequestedBeforeLaunchStopsTheProcess) {
    jobserver::Supervisor supervisor;
    supervisor.cancel();
    auto const start{std::chrono::steady_clock::now()};
    auto const result{supervisor.run(
        helper_command({"sleep", "60000"}),
        std::nullopt,
        std::nullopt,
        [](std::string const&, std::string const&) {},
        [](jobserver::JobHealth, std::string) {},
        [] { return true; })};
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->killed);
    EXPECT_EQ(result->termination_exit_code, 130);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds{2});
}
