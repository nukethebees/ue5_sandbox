#include <sandbox/simulation_benchmark/benchmark_runner.hpp>
#include <sandbox/simulation_benchmark/command_line.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace ml::simulation_benchmark::tests {
namespace {
class TemporaryFile final {
  public:
    TemporaryFile() {
        auto const suffix{std::chrono::steady_clock::now().time_since_epoch().count()};
        path_ = std::filesystem::temp_directory_path() /
                ("sandbox-benchmark-cli-" + std::to_string(suffix) + ".scm");
        std::ofstream{path_} << "(level)";
    }
    ~TemporaryFile() { std::filesystem::remove(path_); }

    auto path() const -> std::filesystem::path const& { return path_; }
  private:
    std::filesystem::path path_{};
};

TEST(SimulationBenchmarkCommandLine, ParsesRequiredOptions) {
    TemporaryFile level;
    auto const path{level.path().string()};
    char const* argv[]{"native-simulation-benchmark", "--level", path.c_str(), "--seconds", "1.5"};

    auto const result{parse_command_line(5, argv)};

    ASSERT_TRUE(result.options.has_value()) << result.standard_error;
    EXPECT_EQ(result.options->level_path, level.path());
    EXPECT_DOUBLE_EQ(result.options->simulated_seconds, 1.5);
}

TEST(SimulationBenchmarkCommandLine, RejectsMissingLevel) {
    char const* argv[]{"native-simulation-benchmark", "--seconds", "1"};
    auto const result{parse_command_line(3, argv)};

    EXPECT_FALSE(result.options.has_value());
    EXPECT_NE(result.exit_code, 0);
}

TEST(SimulationBenchmarkCommandLine, RejectsNonPositiveSeconds) {
    TemporaryFile level;
    auto const path{level.path().string()};
    char const* argv[]{"native-simulation-benchmark", "--level", path.c_str(), "--seconds", "0"};
    auto const result{parse_command_line(5, argv)};

    EXPECT_FALSE(result.options.has_value());
    EXPECT_NE(result.exit_code, 0);
}

TEST(SimulationBenchmarkCommandLine, ReturnsHelpWithoutOptions) {
    char const* argv[]{"native-simulation-benchmark", "--help"};
    auto const result{parse_command_line(2, argv)};

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.standard_output.find("--level"), std::string::npos);
}

TEST(SimulationBenchmarkWorkload, RoundsRequestedSecondsUpToWholeTicks) {
    auto const ticks{calculate_tick_count(1.001)};

    ASSERT_TRUE(ticks.has_value());
    EXPECT_EQ(*ticks, 61);
}

TEST(SimulationBenchmarkWorkload, RejectsInvalidDurations) {
    EXPECT_FALSE(calculate_tick_count(0.0).has_value());
    EXPECT_FALSE(calculate_tick_count(-1.0).has_value());
    EXPECT_FALSE(calculate_tick_count(std::numeric_limits<double>::infinity()).has_value());
}

TEST(SimulationBenchmarkJson, EmitsStableSchemaAndEscapesStrings) {
    BenchmarkResult result;
    result.level_path = "levels/quote\"test.scm";
    result.level_id = "test";
    result.level_title = "line\nbreak";
    result.requested_ticks = 60;
    result.completed_ticks = 60;
    result.elapsed_seconds = 2.0;

    auto const json{to_json(result)};

    EXPECT_NE(json.find("\"schema_version\":1"), std::string::npos);
    EXPECT_NE(json.find("quote\\\"test.scm"), std::string::npos);
    EXPECT_NE(json.find("line\\nbreak"), std::string::npos);
    EXPECT_NE(json.find("\"ticks_per_second\":30"), std::string::npos);
}

TEST(SimulationBenchmarkRunner, LoadsCompilesAndAdvancesExistingLevel) {
    auto const level_path{std::filesystem::path{SANDBOX_PROJECT_SOURCE_DIR} / "LevelScripts" /
                          "DevThreeSecondFailure.scm"};
    auto const result{run_benchmark({.level_path = level_path, .simulated_seconds = 0.01})};

    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->level_id, "dev-three-second-failure");
    EXPECT_EQ(result->requested_ticks, 1);
    EXPECT_EQ(result->completed_ticks, 1);
    EXPECT_EQ(result->alive_entities, 1);
}
} // namespace
} // namespace ml::simulation_benchmark::tests
