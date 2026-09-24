#include <sandbox/perf/benchmark_comparison.hpp>

#include <gtest/gtest.h>
#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace sandbox::perf {
namespace {
constexpr std::string_view csv_header{
    "name,src_file,src_line,total_ns,total_perc,counts,mean_ns,min_ns,max_ns,std_ns\n"};

class TemporaryDirectory final {
  public:
    TemporaryDirectory() {
        path_ = std::filesystem::temp_directory_path() /
                ("tracy-benchmark-compare-" + std::to_string(GetCurrentProcessId()) + "-" +
                 std::to_string(++counter_));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }
    [[nodiscard]] auto path() const -> std::filesystem::path const& { return path_; }
  private:
    inline static int counter_{};
    std::filesystem::path path_{};
};

void write_csv(std::filesystem::path const& path,
               std::initializer_list<std::string_view> const rows) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path};
    output << csv_header;
    for (auto const row : rows) {
        output << row << '\n';
    }
}

auto options_for(std::filesystem::path const& root) -> CompareOptions {
    return {.root = root,
            .level = root / "level.scm",
            .seconds = 1.0,
            .a_preset = "a",
            .b_preset = "b",
            .output_directory = root / "results",
            .output_directory_explicit = true,
            .skip_build = true,
            .connection_timeout_seconds = 1.0,
            .process_timeout_seconds = 1.0,
            .top = 5};
}

auto benchmark_result(int const state = 1, double const time = 10.0) -> Json {
    return {{"workload", {{"completed_ticks", 2}}},
            {"final_state", {{"alive_entities", state}}},
            {"timing", {{"mean_tick_microseconds", time}}}};
}

void write_pair(CompareOptions const& options,
                std::string_view const a_row,
                std::string_view const b_row,
                std::string_view const a_self = {},
                std::string_view const b_self = {}) {
    write_csv(options.output_directory / "a" / "inclusive.csv", {a_row});
    write_csv(options.output_directory / "b" / "inclusive.csv", {b_row});
    write_csv(options.output_directory / "a" / "self.csv", {a_self.empty() ? a_row : a_self});
    write_csv(options.output_directory / "b" / "self.csv", {b_self.empty() ? b_row : b_self});
}

TEST(CommandLine, ParsesRootAndForwardedArguments) {
    char const* const arguments[]{"compare",
                                  "--root",
                                  "C:/repo",
                                  "--level",
                                  "level.scm",
                                  "--seconds",
                                  "2.5",
                                  "--game-speed",
                                  "10",
                                  "--a-preset",
                                  "a",
                                  "--b-preset",
                                  "b",
                                  "--",
                                  "--telemetry"};
    auto const parsed{parse_command_line(std::size(arguments), arguments)};
    ASSERT_TRUE(parsed.options.has_value()) << parsed.standard_error;
    EXPECT_EQ(parsed.options->seconds, 2.5);
    EXPECT_EQ(parsed.options->game_speed, 10);
    ASSERT_EQ(parsed.options->runner_arguments.size(), 1);
    EXPECT_EQ(parsed.options->runner_arguments.front(), "--telemetry");
    auto const arguments_for_benchmark{benchmark_arguments(*parsed.options)};
    EXPECT_NE(std::ranges::find(arguments_for_benchmark, "--wait-for-profiler"),
              arguments_for_benchmark.end());
}

TEST(CommandLine, RejectsManagedForwardedArguments) {
    char const* const arguments[]{"compare",
                                  "--root",
                                  "C:/repo",
                                  "--level",
                                  "level.scm",
                                  "--seconds",
                                  "1",
                                  "--a-preset",
                                  "a",
                                  "--b-preset",
                                  "b",
                                  "--",
                                  "--seconds",
                                  "3"};
    auto const parsed{parse_command_line(std::size(arguments), arguments)};
    EXPECT_FALSE(parsed.options.has_value());
    EXPECT_EQ(parsed.exit_code, 2);
}

TEST(Csv, ParsesStatisticsAndNormalizesSourcePaths) {
    TemporaryDirectory directory;
    auto const path{directory.path() / "zones.csv"};
    write_csv(
        path,
        {"Tick," + directory.path().generic_string() + "/native/sim.cpp,20,100,50,2,50,40,60,10"});
    auto const parsed{parse_zone_csv(path, directory.path())};
    ASSERT_EQ(parsed.zones.size(), 1);
    auto const& zone{parsed.zones.begin()->second};
    EXPECT_EQ(zone.source_file, "native/sim.cpp");
    EXPECT_EQ(zone.count, 2);
    EXPECT_EQ(zone.total_nanoseconds, 100);
}

TEST(Csv, RejectsAmbiguousIdentityAndMalformedInput) {
    TemporaryDirectory directory;
    auto const duplicate{directory.path() / "duplicate.csv"};
    write_csv(duplicate,
              {"Tick,native/sim.cpp,20,100,50,2,50,40,60,10",
               "Tick,native/sim.cpp,40,100,50,2,50,40,60,10"});
    auto const parsed{parse_zone_csv(duplicate, directory.path())};
    EXPECT_TRUE(parsed.zones.empty());
    ASSERT_EQ(parsed.warnings.size(), 1);
    EXPECT_EQ(parsed.warnings.front().code, "ambiguous_zone_identity");
    std::ofstream{directory.path() / "bad.csv"} << "name,total_ns\nTick,10\n";
    EXPECT_THROW(static_cast<void>(parse_zone_csv(directory.path() / "bad.csv", directory.path())),
                 PipelineError);
    write_csv(directory.path() / "bad-row.csv", {"Tick,native/sim.cpp,20,100"});
    EXPECT_THROW(
        static_cast<void>(parse_zone_csv(directory.path() / "bad-row.csv", directory.path())),
        PipelineError);
}

TEST(Csv, PreservesMultipleDistinctZoneRecords) {
    TemporaryDirectory directory;
    auto const path{directory.path() / "zones.csv"};
    write_csv(path,
              {"Tick,native/sim.cpp,20,100,50,2,50,40,60,10",
               "Render,native/render.cpp,40,200,25,4,50,30,70,10"});
    auto const parsed{parse_zone_csv(path, directory.path())};
    EXPECT_EQ(parsed.zones.size(), 2);
}

TEST(Comparison, MatchesMovedLinesAndProducesDeterministicRegression) {
    TemporaryDirectory directory;
    auto options{options_for(directory.path())};
    write_pair(options,
               "Tick,native/sim.cpp,20,100,50,2,50,40,60,10",
               "Tick,native/sim.cpp,90,120,50,2,60,40,70,10");
    auto const comparison{compare_results(options, benchmark_result(), benchmark_result())};
    EXPECT_EQ(comparison.status, ComparisonStatus::complete);
    ASSERT_EQ(comparison.zones.size(), 1);
    EXPECT_DOUBLE_EQ(*comparison.zones.front().inclusive_delta_percent, 20.0);
    auto const json = to_json(comparison);
    ASSERT_TRUE(json.is_object()) << json.dump();
    ASSERT_TRUE(json.at("zones").is_array()) << json.dump();
    ASSERT_FALSE(json.at("zones").empty());
    ASSERT_TRUE(json.at("zones").front().is_object()) << json.dump();
    EXPECT_EQ(json["zones"][0]["identity"]["source_file"], "native/sim.cpp");
    std::ostringstream output;
    print_summary(comparison, options.output_directory, 5, output);
    EXPECT_NE(output.str().find("Largest regressions"), std::string::npos);
}

TEST(Comparison, ReportsMissingAndCountMismatchZones) {
    TemporaryDirectory directory;
    auto options{options_for(directory.path())};
    write_pair(options,
               "Tick,native/sim.cpp,20,100,50,2,50,40,60,10",
               "Tick,native/sim.cpp,20,100,50,3,50,40,60,10");
    auto comparison{compare_results(options, benchmark_result(), benchmark_result())};
    EXPECT_EQ(comparison.status, ComparisonStatus::complete_with_warnings);
    EXPECT_FALSE(comparison.zones.front().comparable);
    write_csv(options.output_directory / "b" / "self.csv", {});
    comparison = compare_results(options, benchmark_result(), benchmark_result());
    EXPECT_TRUE(std::ranges::any_of(comparison.warnings, [](Warning const& warning) {
        return warning.code == "missing_zone_b";
    }));
}

TEST(Comparison, ReportsImprovementsNoChangeAndIncomparableFinalState) {
    TemporaryDirectory directory;
    auto options{options_for(directory.path())};
    write_pair(options,
               "Tick,native/sim.cpp,20,100,50,2,50,40,60,10",
               "Tick,native/sim.cpp,20,80,50,2,40,30,50,10");
    auto comparison{compare_results(options, benchmark_result(), benchmark_result())};
    EXPECT_DOUBLE_EQ(*comparison.zones.front().inclusive_delta_percent, -20.0);
    std::ostringstream output;
    print_summary(comparison, options.output_directory, 5, output);
    EXPECT_NE(output.str().find("Largest improvements"), std::string::npos);
    comparison = compare_results(options, benchmark_result(1, 10.0), benchmark_result(2, 10.0));
    EXPECT_EQ(comparison.status, ComparisonStatus::incomparable);
    EXPECT_EQ(percentage_delta(0.0, 1.0), std::nullopt);
    EXPECT_EQ(percentage_delta(10.0, 10.0), 0.0);
}

TEST(Application, RejectsNonemptyExplicitOutputDirectoryWithoutSubmittingAJob) {
    TemporaryDirectory directory;
    auto const output{directory.path() / "result"};
    std::filesystem::create_directories(output);
    std::ofstream{output / "keep.txt"} << "keep";
    std::vector<std::string> arguments{"compare",
                                       "--root",
                                       directory.path().string(),
                                       "--level",
                                       "level.scm",
                                       "--seconds",
                                       "1",
                                       "--a-preset",
                                       "a",
                                       "--b-preset",
                                       "b",
                                       "--output-dir",
                                       output.string(),
                                       "--skip-build"};
    std::vector<char const*> raw_arguments;
    for (auto const& argument : arguments) {
        raw_arguments.push_back(argument.c_str());
    }
    std::ostringstream standard_output;
    std::ostringstream standard_error;
    EXPECT_EQ(run_application(static_cast<int>(raw_arguments.size()),
                              raw_arguments.data(),
                              standard_output,
                              standard_error),
              1);
    EXPECT_EQ(std::ifstream{output / "keep.txt"}.get(), 'k');
    EXPECT_NE(standard_error.str().find("output directory is not empty"), std::string::npos);
}

TEST(Process, WritesStructuredFailureWhenBenchmarkExitsBeforeProfilerReady) {
    TemporaryDirectory directory;
    auto const root{directory.path()};
    auto const helper{std::filesystem::path{IOJ_PERF_TEST_HELPER_PATH}};
    auto const benchmark{root / "out" / "build" / "a" / "bin" / "native-simulation-benchmark.exe"};
    auto const capture{root / "out" / "build" / "tracy-tools" / "bin" / "tracy-capture.exe"};
    auto const exporter{root / "out" / "build" / "tracy-tools" / "bin" / "tracy-csvexport.exe"};
    std::filesystem::create_directories(benchmark.parent_path());
    std::filesystem::create_directories(capture.parent_path());
    std::filesystem::copy_file(helper, benchmark);
    std::filesystem::copy_file(helper, capture);
    std::filesystem::copy_file(helper, exporter);
    std::ofstream{root / "level.scm"} << "(level)";
    auto const output{root / "result"};
    std::vector<std::string> arguments{"compare",
                                       "--root",
                                       root.string(),
                                       "--level",
                                       (root / "level.scm").string(),
                                       "--seconds",
                                       "1",
                                       "--a-preset",
                                       "a",
                                       "--b-preset",
                                       "a",
                                       "--output-dir",
                                       output.string(),
                                       "--skip-build",
                                       "--jobserver-child",
                                       "--",
                                       "exit",
                                       "7"};
    std::vector<char const*> raw_arguments;
    for (auto const& argument : arguments) {
        raw_arguments.push_back(argument.c_str());
    }
    SetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_JOB", L"test");
    std::ostringstream standard_output;
    std::ostringstream standard_error;
    auto const exit_code{run_application(static_cast<int>(raw_arguments.size()),
                                         raw_arguments.data(),
                                         standard_output,
                                         standard_error)};
    SetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_JOB", nullptr);
    EXPECT_EQ(exit_code, 1);
    auto const failure = Json::parse(std::ifstream{output / "comparison.json"});
    ASSERT_TRUE(failure.is_object()) << failure.dump();
    EXPECT_EQ(failure.at("status"), "failed");
    EXPECT_EQ(failure.at("warnings").at(0).at("code"), "benchmark_failed");
}

TEST(Process, StopsCaptureBeforeJoiningReadersWhenBenchmarkTimesOut) {
    TemporaryDirectory directory;
    auto const root{directory.path()};
    auto const helper{std::filesystem::path{IOJ_PERF_TEST_HELPER_PATH}};
    auto const benchmark{root / "out" / "build" / "a" / "bin" / "native-simulation-benchmark.exe"};
    auto const capture{root / "out" / "build" / "tracy-tools" / "bin" / "tracy-capture.exe"};
    auto const exporter{root / "out" / "build" / "tracy-tools" / "bin" / "tracy-csvexport.exe"};
    std::filesystem::create_directories(benchmark.parent_path());
    std::filesystem::create_directories(capture.parent_path());
    std::filesystem::copy_file(helper, benchmark);
    std::filesystem::copy_file(helper, capture);
    std::filesystem::copy_file(helper, exporter);
    std::ofstream{root / "level.scm"} << "(level)";
    auto const output{root / "result"};
    std::vector<std::string> arguments{"compare",
                                       "--root",
                                       root.string(),
                                       "--level",
                                       (root / "level.scm").string(),
                                       "--seconds",
                                       "1",
                                       "--a-preset",
                                       "a",
                                       "--b-preset",
                                       "a",
                                       "--output-dir",
                                       output.string(),
                                       "--process-timeout-seconds",
                                       "0.2",
                                       "--skip-build",
                                       "--jobserver-child",
                                       "--",
                                       "ready-and-block"};
    std::vector<char const*> raw_arguments;
    for (auto const& argument : arguments) {
        raw_arguments.push_back(argument.c_str());
    }

    SetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_JOB", L"test");
    std::ostringstream standard_output;
    std::ostringstream standard_error;
    auto const started{std::chrono::steady_clock::now()};
    auto const exit_code{run_application(static_cast<int>(raw_arguments.size()),
                                         raw_arguments.data(),
                                         standard_output,
                                         standard_error)};
    auto const elapsed{std::chrono::steady_clock::now() - started};
    SetEnvironmentVariableW(L"NUKETHEBEES_JOBSERVER_JOB", nullptr);

    EXPECT_EQ(exit_code, 1);
    EXPECT_LT(elapsed, std::chrono::seconds{5});
    EXPECT_TRUE(std::filesystem::is_regular_file(output / "a" / "capture.tracy.started"));
    auto const failure = Json::parse(std::ifstream{output / "comparison.json"});
    ASSERT_TRUE(failure.is_object()) << failure.dump();
    EXPECT_EQ(failure.at("status"), "failed");
    EXPECT_EQ(failure.at("warnings").at(0).at("code"), "benchmark_timeout");
}
} // namespace
} // namespace sandbox::perf
