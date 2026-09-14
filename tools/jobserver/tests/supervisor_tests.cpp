#include "supervisor.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <mutex>
#include <string>
#include <vector>

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

TEST(JobserverSupervisor, DoesNotCreateAConsoleWindow) {
    jobserver::Supervisor supervisor;
    auto const result{supervisor.run(
        helper_command({"require-no-console"}),
        std::nullopt,
        std::nullopt,
        [](std::string const&, std::string const&) {},
        [](jobserver::JobHealth, std::string) {},
        [] { return true; })};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->exit_code, 0);
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

TEST(JobserverSupervisor, PreservesWindowsArgumentsIncludingUnicodeAndEmptyValues) {
    std::vector<std::string> const arguments{
        "", "plain", "two words", "a\"b", "ends with\\", "snowman-\xe2\x98\x83"};
    auto command_arguments{arguments};
    command_arguments.insert(command_arguments.begin(), "arguments");
    jobserver::Supervisor supervisor;
    std::string output;
    auto const result{supervisor.run(
        helper_command(std::move(command_arguments)),
        std::nullopt,
        std::nullopt,
        [&](std::string const& stream, std::string const& text) {
            if (stream == "stdout") {
                output += text;
            }
        },
        [](jobserver::JobHealth, std::string) {},
        [] { return true; })};
    ASSERT_TRUE(result.has_value());
    std::string expected;
    for (auto const& argument : arguments) {
        expected += std::to_string(argument.size()) + ':' + argument + "\r\n";
    }
    EXPECT_EQ(output, expected);
}

TEST(JobserverSupervisor, AppliesEnvironmentOverridesCaseInsensitively) {
    ASSERT_EQ(_putenv_s("JOBSERVER_TEST_OVERRIDE", "old"), 0);
    auto command{helper_command({"environment", "JOBSERVER_TEST_OVERRIDE"})};
    command.environment.push_back(
        {.name = "jobserver_test_override", .value = "snowman-\xe2\x98\x83"});
    jobserver::Supervisor supervisor;
    std::string output;
    auto const result{supervisor.run(
        command,
        std::nullopt,
        std::nullopt,
        [&](std::string const&, std::string const& text) { output += text; },
        [](jobserver::JobHealth, std::string) {},
        [] { return true; })};
    static_cast<void>(_putenv_s("JOBSERVER_TEST_OVERRIDE", ""));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(output, "snowman-\xe2\x98\x83");
}

TEST(JobserverSupervisor, UsesWorkingDirectoryAndUnicodeExecutablePath) {
    auto const root{std::filesystem::temp_directory_path() / L"jobserver working \u2603"};
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    auto const executable{root / L"helper \u2603.exe"};
    std::filesystem::copy_file(JOBSERVER_TEST_HELPER_PATH, executable);
    auto command{helper_command({"working-directory"})};
    command.executable = executable;
    command.working_directory = root;
    jobserver::Supervisor supervisor;
    std::string output;
    auto const result{supervisor.run(
        command,
        std::nullopt,
        std::nullopt,
        [&](std::string const&, std::string const& text) { output += text; },
        [](jobserver::JobHealth, std::string) {},
        [] { return true; })};
    std::filesystem::remove_all(root);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(output.find("jobserver working"), std::string::npos);
    EXPECT_NE(output.find("\xe2\x98\x83"), std::string::npos);
}

TEST(JobserverSupervisor, PreservesLargeAndBinaryOutput) {
    jobserver::Supervisor supervisor;
    std::string output;
    auto const large_result{supervisor.run(
        helper_command({"large-output", "65537"}),
        std::nullopt,
        std::nullopt,
        [&](std::string const&, std::string const& text) { output += text; },
        [](jobserver::JobHealth, std::string) {},
        [] { return true; })};
    ASSERT_TRUE(large_result.has_value());
    EXPECT_EQ(output, std::string(65'537, 'x'));

    jobserver::Supervisor binary_supervisor;
    output.clear();
    auto const binary_result{binary_supervisor.run(
        helper_command({"binary-output"}),
        std::nullopt,
        std::nullopt,
        [&](std::string const&, std::string const& text) { output += text; },
        [](jobserver::JobHealth, std::string) {},
        [] { return true; })};
    ASSERT_TRUE(binary_result.has_value());
    EXPECT_EQ(output, std::string("a\0b\xff", 4));
}

TEST(JobserverSupervisor, WaitsForDescendantAfterRootExits) {
    jobserver::Supervisor supervisor;
    std::string output;
    auto const result{supervisor.run(
        helper_command({"spawn-output"}),
        std::nullopt,
        std::nullopt,
        [&](std::string const&, std::string const& text) { output += text; },
        [](jobserver::JobHealth, std::string) {},
        [] { return true; })};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->exit_code, 0);
    EXPECT_NE(output.find("descendant output"), std::string::npos);
}

TEST(JobserverSupervisor, ClearsHangSuspicionWhenOutputResumes) {
    jobserver::Supervisor supervisor;
    std::vector<jobserver::JobHealth> health_changes;
    auto const result{supervisor.run(
        helper_command({"pause-output"}),
        std::nullopt,
        std::chrono::milliseconds{50},
        [](std::string const&, std::string const&) {},
        [&](jobserver::JobHealth const health, std::string) { health_changes.push_back(health); },
        [] { return true; })};
    ASSERT_TRUE(result.has_value());
    ASSERT_GE(health_changes.size(), 2U);
    EXPECT_EQ(health_changes.front(), jobserver::JobHealth::suspected_hang);
    EXPECT_EQ(health_changes.back(), jobserver::JobHealth::normal);
}

TEST(JobserverSupervisor, HardKillUsesDistinctExitCode) {
    jobserver::Supervisor supervisor;
    auto result_future{std::async(std::launch::async, [&] {
        return supervisor.run(
            helper_command({"sleep", "60000"}),
            std::nullopt,
            std::nullopt,
            [](std::string const&, std::string const&) {},
            [](jobserver::JobHealth, std::string) {},
            [] { return true; });
    })};
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    supervisor.kill();
    auto const result{result_future.get()};
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->killed);
    EXPECT_EQ(result->termination_exit_code, 137);
}
