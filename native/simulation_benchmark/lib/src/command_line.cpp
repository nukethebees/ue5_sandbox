#include <sandbox/simulation_benchmark/command_line.hpp>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ml::simulation_benchmark {
auto parse_command_line(int const argc, char const* const* argv) -> CommandLineResult {
    BenchmarkOptions options;
    CLI::App app{"Run a Lisp-authored level through the deterministic native simulation."};
    app.add_option("--level", options.level_path, "Lisp level file")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--seconds", options.simulated_seconds, "In-game seconds to simulate")
        ->required()
        ->check(CLI::PositiveNumber);
    app.add_option("--game-speed", options.game_speed, "Requested initial simulation time scale")
        ->check(CLI::PositiveNumber);
    app.add_option("--wait-for-profiler",
                   options.profiler_connection_timeout_seconds,
                   "Wait for a profiler connection before timing, up to this many seconds")
        ->check(CLI::PositiveNumber);
    app.add_option("--fighter-stress-cap",
                   options.fighter_stress_cap,
                   "Use the production fighter cap and require exact steady-state saturation")
        ->check(CLI::PositiveNumber);
    app.add_option("--fighter-stress-caps",
                   options.fighter_stress_caps,
                   "Run one fighter stress benchmark for each cap in this process")
        ->expected(1, -1)
        ->delimiter(',')
        ->check(CLI::PositiveNumber);
    auto* const warmup_option{app.add_option(
        "--warmup-seconds", options.warmup_seconds, "Post-saturation simulated warm-up seconds")};
    auto* const saturation_timeout_option{
        app.add_option("--saturation-timeout-seconds",
                       options.saturation_timeout_seconds,
                       "Maximum simulated seconds allowed to reach the fighter cap")
            ->check(CLI::PositiveNumber)};
    app.add_flag("--telemetry", options.telemetry_enabled, "Capture level telemetry");

    try {
        app.parse(argc, argv);
    } catch (CLI::ParseError const& error) {
        std::ostringstream standard_output;
        std::ostringstream standard_error;
        auto const exit_code{app.exit(error, standard_output, standard_error)};
        return {.standard_output = standard_output.str(),
                .standard_error = standard_error.str(),
                .exit_code = exit_code};
    }

    if (!std::isfinite(options.simulated_seconds)) {
        return {.standard_error = "--seconds must be finite\n", .exit_code = 2};
    }
    if (options.profiler_connection_timeout_seconds.has_value() &&
        !std::isfinite(*options.profiler_connection_timeout_seconds)) {
        return {.standard_error = "--wait-for-profiler must be finite\n", .exit_code = 2};
    }
    if (!std::isfinite(options.warmup_seconds) || options.warmup_seconds < 0.0) {
        return {.standard_error = "--warmup-seconds must be finite and non-negative\n",
                .exit_code = 2};
    }
    if (!std::isfinite(options.saturation_timeout_seconds)) {
        return {.standard_error = "--saturation-timeout-seconds must be finite\n", .exit_code = 2};
    }
    if (options.fighter_stress_cap.has_value() && !options.fighter_stress_caps.empty()) {
        return {.standard_error = "--fighter-stress-cap and --fighter-stress-caps are mutually "
                                  "exclusive\n",
                .exit_code = 2};
    }
    for (auto const cap : options.fighter_stress_caps) {
        if (std::ranges::count(options.fighter_stress_caps, cap) > 1) {
            return {.standard_error = "--fighter-stress-caps must contain unique values\n",
                    .exit_code = 2};
        }
    }
    if (!options.fighter_stress_cap.has_value() && options.fighter_stress_caps.empty() &&
        (warmup_option->count() > 0 || saturation_timeout_option->count() > 0)) {
        return {.standard_error = "--warmup-seconds and --saturation-timeout-seconds require "
                                  "--fighter-stress-cap or --fighter-stress-caps\n",
                .exit_code = 2};
    }
    return {.options = std::move(options)};
}
} // namespace ml::simulation_benchmark
