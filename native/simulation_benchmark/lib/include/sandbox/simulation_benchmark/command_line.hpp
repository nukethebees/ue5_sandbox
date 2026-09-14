#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace ml::simulation_benchmark {
struct BenchmarkOptions {
    std::filesystem::path level_path{};
    double simulated_seconds{};
    std::uint32_t game_speed{1};
    bool telemetry_enabled{};
    bool detailed_timing{};
};

struct CommandLineResult {
    std::optional<BenchmarkOptions> options{};
    std::string standard_output{};
    std::string standard_error{};
    int exit_code{};
};

[[nodiscard]] auto parse_command_line(int argc, char const* const* argv) -> CommandLineResult;
} // namespace ml::simulation_benchmark
