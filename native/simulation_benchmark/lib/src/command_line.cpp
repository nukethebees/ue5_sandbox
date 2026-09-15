#include <sandbox/simulation_benchmark/command_line.hpp>

#include <CLI/CLI.hpp>

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
    app.add_option("--game-speed", options.game_speed, "Deterministic ticks per advance call")
        ->check(CLI::PositiveNumber);
    app.add_option("--wait-for-profiler",
                   options.profiler_connection_timeout_seconds,
                   "Wait for a profiler connection before timing, up to this many seconds")
        ->check(CLI::PositiveNumber);
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
    return {.options = std::move(options)};
}
} // namespace ml::simulation_benchmark
