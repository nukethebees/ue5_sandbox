#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace sandbox::perf {
using Json = nlohmann::json;

struct CompareOptions {
    std::filesystem::path root{};
    std::filesystem::path level{};
    double seconds{};
    int game_speed{1};
    std::string a_preset{};
    std::string b_preset{};
    std::filesystem::path output_directory{};
    bool output_directory_explicit{};
    bool skip_build{};
    double connection_timeout_seconds{30.0};
    double process_timeout_seconds{1800.0};
    int top{5};
    std::vector<std::string> runner_arguments{};
    bool jobserver_child{};
};

struct CommandLineResult {
    std::optional<CompareOptions> options{};
    std::string standard_output{};
    std::string standard_error{};
    int exit_code{};
};

struct ZoneKey {
    std::string name{};
    std::string source_file{};

    auto operator<=>(ZoneKey const&) const = default;
};

struct ZoneStatistics {
    std::string name{};
    std::string source_file{};
    int source_line{};
    std::int64_t total_nanoseconds{};
    double total_percent{};
    std::int64_t count{};
    std::int64_t mean_nanoseconds{};
    std::int64_t minimum_nanoseconds{};
    std::int64_t maximum_nanoseconds{};
};

struct Warning {
    std::string code{};
    std::string severity{};
    std::string message{};
    Json details = Json::object();
};

struct ParsedZones {
    std::map<ZoneKey, ZoneStatistics> zones{};
    std::vector<Warning> warnings{};
};

enum class ComparisonStatus {
    complete,
    complete_with_warnings,
    incomparable,
    failed,
};

struct ZoneComparison {
    ZoneKey identity{};
    std::optional<ZoneStatistics> a_inclusive{};
    std::optional<ZoneStatistics> b_inclusive{};
    std::optional<ZoneStatistics> a_self{};
    std::optional<ZoneStatistics> b_self{};
    std::optional<double> inclusive_delta_percent{};
    std::optional<double> self_delta_percent{};
    bool counts_match{};
    bool comparable{};
};

struct Comparison {
    ComparisonStatus status{};
    Json a_result{};
    Json b_result{};
    std::optional<double> overall_delta_percent{};
    std::vector<ZoneComparison> zones{};
    std::vector<Warning> warnings{};
};

class PipelineError final : public std::runtime_error {
  public:
    PipelineError(std::string code, std::string message);

    [[nodiscard]] auto code() const -> std::string const&;
  private:
    std::string code_;
};

[[nodiscard]] auto parse_command_line(int argc, char const* const* argv) -> CommandLineResult;
[[nodiscard]] auto benchmark_arguments(CompareOptions const& options) -> std::vector<std::string>;
[[nodiscard]] auto normalize_source_file(std::string source_file, std::filesystem::path const& root)
    -> std::string;
[[nodiscard]] auto parse_zone_csv(std::filesystem::path const& path,
                                  std::filesystem::path const& root) -> ParsedZones;
[[nodiscard]] auto percentage_delta(double a, double b) -> std::optional<double>;
[[nodiscard]] auto compare_results(CompareOptions const& options,
                                   Json const& a_result,
                                   Json const& b_result) -> Comparison;
[[nodiscard]] auto to_json(Comparison const& comparison) -> Json;
void print_summary(Comparison const& comparison,
                   std::filesystem::path const& output_directory,
                   int top,
                   std::ostream& output);
[[nodiscard]] auto run_application(int argc,
                                   char const* const* argv,
                                   std::ostream& standard_output,
                                   std::ostream& standard_error) -> int;
} // namespace sandbox::perf
