#include <sandbox/simulation_benchmark/benchmark_runner.hpp>
#include <sandbox/simulation_benchmark/command_line.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

namespace ioj::simulation_benchmark::tests {
namespace {
class TemporaryFile final {
  public:
    TemporaryFile() {
        auto const suffix{std::chrono::steady_clock::now().time_since_epoch().count()};
        path_ = std::filesystem::temp_directory_path() /
                ("sandbox-benchmark-cli-" + std::to_string(suffix) + ".scm");
        std::ofstream{path_} << "(level)";
    }
    ~TemporaryFile() {
        std::error_code ignored{};
        std::filesystem::remove(path_, ignored);
    }

    auto path() const -> std::filesystem::path const& { return path_; }
  private:
    std::filesystem::path path_{};
};

class SimulationBenchmarkCommandLine : public testing::Test {
  protected:
    auto parse(std::vector<char const*> const& arguments) const -> CommandLineResult {
        auto const path{level_.path().string()};
        std::vector<char const*> argv{"native-simulation-benchmark", "--level", path.c_str()};
        argv.insert(argv.end(), arguments.begin(), arguments.end());
        return parse_command_line(static_cast<int>(argv.size()), argv.data());
    }

    TemporaryFile level_;
};

TEST_F(SimulationBenchmarkCommandLine, ParsesRequiredOptions) {
    auto const result{parse(
        {"--seconds", "1.5", "--game-speed", "100", "--wait-for-profiler", "5", "--telemetry"})};

    ASSERT_TRUE(result.options.has_value()) << result.standard_error;
    EXPECT_EQ(result.options->level_path, level_.path());
    EXPECT_DOUBLE_EQ(result.options->simulated_seconds, 1.5);
    EXPECT_EQ(result.options->game_speed, 100u);
    ASSERT_TRUE(result.options->profiler_connection_timeout_seconds.has_value());
    EXPECT_DOUBLE_EQ(*result.options->profiler_connection_timeout_seconds, 5.0);
    EXPECT_TRUE(result.options->telemetry_enabled);
}

TEST_F(SimulationBenchmarkCommandLine, ParsesFighterStressOptions) {
    auto const result{parse({"--seconds",
                             "10",
                             "--fighter-stress-cap",
                             "4000",
                             "--warmup-seconds",
                             "5",
                             "--saturation-timeout-seconds",
                             "30"})};

    ASSERT_TRUE(result.options.has_value()) << result.standard_error;
    ASSERT_TRUE(result.options->fighter_stress_cap.has_value());
    EXPECT_EQ(*result.options->fighter_stress_cap, 4000);
    EXPECT_DOUBLE_EQ(result.options->warmup_seconds, 5.0);
    EXPECT_DOUBLE_EQ(result.options->saturation_timeout_seconds, 30.0);
}

TEST_F(SimulationBenchmarkCommandLine, ParsesOrderedCommaSeparatedFighterStressCaps) {
    auto const result{parse({"--seconds",
                             "10",
                             "--fighter-stress-caps",
                             "4000,2000",
                             "--warmup-seconds",
                             "5",
                             "--saturation-timeout-seconds",
                             "30"})};

    ASSERT_TRUE(result.options.has_value()) << result.standard_error;
    EXPECT_FALSE(result.options->fighter_stress_cap.has_value());
    ASSERT_EQ(result.options->fighter_stress_caps.size(), 2u);
    EXPECT_EQ(result.options->fighter_stress_caps[0], 4000);
    EXPECT_EQ(result.options->fighter_stress_caps[1], 2000);
}

TEST(SimulationBenchmarkCommandLineWithoutLevel, RejectsMissingLevel) {
    char const* argv[]{"native-simulation-benchmark", "--seconds", "1"};
    auto const result{parse_command_line(3, argv)};

    EXPECT_FALSE(result.options.has_value());
    EXPECT_NE(result.exit_code, 0);
}

struct InvalidArguments {
    char const* name;
    std::vector<char const*> arguments;
    std::string expected_error{};
};

class InvalidSimulationBenchmarkCommandLine
    : public SimulationBenchmarkCommandLine
    , public testing::WithParamInterface<InvalidArguments> {};

TEST_P(InvalidSimulationBenchmarkCommandLine, RejectsInvalidOptions) {
    auto const& test{GetParam()};
    auto const result{parse(test.arguments)};

    EXPECT_FALSE(result.options.has_value());
    EXPECT_NE(result.exit_code, 0);
    if (!test.expected_error.empty()) {
        EXPECT_NE(result.standard_error.find(test.expected_error), std::string::npos);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Options,
    InvalidSimulationBenchmarkCommandLine,
    testing::Values(
        InvalidArguments{"DuplicateStressCaps",
                         {"--seconds", "1", "--fighter-stress-caps", "12", "12"}},
        InvalidArguments{"ZeroStressCap", {"--seconds", "1", "--fighter-stress-caps", "0"}},
        InvalidArguments{"OverflowStressCap",
                         {"--seconds", "1", "--fighter-stress-caps", "2147483648"}},
        InvalidArguments{"FractionalStressCap", {"--seconds", "1", "--fighter-stress-caps", "1.5"}},
        InvalidArguments{"NonPositiveSeconds", {"--seconds", "0"}},
        InvalidArguments{"NonPositiveGameSpeed", {"--seconds", "1", "--game-speed", "0"}},
        InvalidArguments{"NonPositiveProfilerWait", {"--seconds", "1", "--wait-for-profiler", "0"}},
        InvalidArguments{"StressSetupWithoutFighterCap",
                         {"--seconds", "1", "--warmup-seconds", "5"},
                         "--fighter-stress-cap or --fighter-stress-caps"}),
    [](auto const& info) { return info.param.name; });

TEST(SimulationBenchmarkCommandLineWithoutLevel, ReturnsHelpWithoutOptions) {
    char const* argv[]{"native-simulation-benchmark", "--help"};
    auto const result{parse_command_line(2, argv)};

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.standard_output.find("--level"), std::string::npos);
}

TEST(SimulationBenchmarkWorkload, RoundsRequestedSecondsUpToWholeTicks) {
    auto const ticks{calculate_tick_count(1.001)};

    ASSERT_TRUE(ticks.has_value());
    EXPECT_EQ(ticks->value(), 61);
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
    result.requested_ticks = sim::SimTick{60};
    result.completed_ticks = sim::SimTick{60};
    result.game_speed = 4;
    result.advance_calls = 15;
    result.elapsed_seconds = 2.0;
    result.total_elapsed_seconds = 3.0;
    result.measured_ticks = sim::SimTick{60};
    result.total_simulation_ticks = sim::SimTick{90};
    result.median_tick_microseconds = 25.0;
    result.p95_tick_microseconds = 40.0;
    result.p99_tick_microseconds = 50.0;
    result.fighter_stress_enabled = true;
    result.configured_fighter_cap = 2000;
    result.steady_state_fighters = 2000;
    result.minimum_measured_fighters = 2000;
    result.maximum_measured_fighters = 2000;
    result.attacking_fighters = 2000;
    result.frame_memory_peak_payload_bytes = 123;
    result.frame_memory_total_root_claims = 456;

    auto const json{to_json(result)};

    EXPECT_NE(json.find("\"schema_version\":2"), std::string::npos);
    EXPECT_NE(json.find("quote\\\"test.scm"), std::string::npos);
    EXPECT_NE(json.find("line\\nbreak"), std::string::npos);
    EXPECT_NE(json.find("\"ticks_per_second\":30"), std::string::npos);
    EXPECT_NE(json.find("\"game_speed\":4"), std::string::npos);
    EXPECT_NE(json.find("\"advance_calls\":15"), std::string::npos);
    EXPECT_NE(json.find("\"frame_peak_payload_bytes\":123"), std::string::npos);
    EXPECT_NE(json.find("\"frame_total_root_claims\":456"), std::string::npos);
    EXPECT_NE(json.find("\"median_tick_microseconds\":25"), std::string::npos);
    EXPECT_NE(json.find("\"configured_cap\":2000"), std::string::npos);
    EXPECT_NE(json.find("\"attack\":2000"), std::string::npos);
}

TEST(SimulationBenchmarkRunner, LoadsCompilesAndAdvancesExistingLevel) {
    auto const level_path{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} / "LevelScripts" /
                          "DevThreeSecondFailure.scm"};
    auto const result{run_benchmark({.level_path = level_path, .simulated_seconds = 0.01})};

    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->level_id, "dev-three-second-failure");
    EXPECT_EQ(result->requested_ticks.value(), 1);
    EXPECT_EQ(result->completed_ticks.value(), 1);
    EXPECT_EQ(result->alive_entities, 1);
}

TEST(SimulationBenchmarkRunner, AdvancesOneTickPerCallAtAnyRequestedGameSpeed) {
    auto const level_path{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} / "LevelScripts" /
                          "DevThreeSecondFailure.scm"};
    auto const result{
        run_benchmark({.level_path = level_path, .simulated_seconds = 0.05, .game_speed = 2})};

    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->requested_ticks.value(), 3);
    EXPECT_EQ(result->completed_ticks.value(), 3);
    EXPECT_EQ(result->advance_calls, 3u);
}

TEST(SimulationBenchmarkRunner, SaturatesAndMeasuresFighterStressScenario) {
    auto const level_path{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} / "LevelScripts" /
                          "FighterSchedulingBenchmark.scm"};
    auto const result{run_benchmark({.level_path = level_path,
                                     .simulated_seconds = 0.01,
                                     .fighter_stress_cap = 12,
                                     .warmup_seconds = 0.0,
                                     .saturation_timeout_seconds = 1.0})};

    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_TRUE(result->fighter_stress_enabled);
    EXPECT_EQ(result->configured_fighter_cap, 12);
    EXPECT_EQ(result->initial_capital_ships, 512);
    EXPECT_EQ(result->steady_state_fighters, 12);
    EXPECT_EQ(result->minimum_measured_fighters, 12);
    EXPECT_EQ(result->maximum_measured_fighters, 12);
    EXPECT_EQ(result->fighter_spawns_during_measurement, 0);
    EXPECT_EQ(result->measured_ticks.value(), 1);
    EXPECT_GT(result->total_simulation_ticks, result->measured_ticks);
}
} // namespace
} // namespace ioj::simulation_benchmark::tests
