#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ml::simulation_benchmark {
struct BenchmarkOptions {
    std::filesystem::path level_path{};
    double simulated_seconds{};
    std::uint32_t game_speed{1};
    std::optional<double> profiler_connection_timeout_seconds{};
    std::optional<std::int32_t> fighter_stress_cap{};
    std::vector<std::int32_t> fighter_stress_caps{};
    double warmup_seconds{5.0};
    double saturation_timeout_seconds{60.0};
    bool telemetry_enabled{};
};

struct CommandLineResult {
    std::optional<BenchmarkOptions> options{};
    std::string standard_output{};
    std::string standard_error{};
    int exit_code{};
};

[[nodiscard]] auto parse_command_line(int argc, char const* const* argv) -> CommandLineResult;
} // namespace ml::simulation_benchmark
