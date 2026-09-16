#include <sandbox/simulation_benchmark/benchmark_runner.hpp>
#include <sandbox/simulation_benchmark/command_line.hpp>

#include <iostream>

auto main(int const argc, char const* const* argv) -> int {
    auto command_line{ml::simulation_benchmark::parse_command_line(argc, argv)};
    if (!command_line.standard_output.empty()) {
        std::cout << command_line.standard_output;
    }
    if (!command_line.standard_error.empty()) {
        std::cerr << command_line.standard_error;
    }
    if (!command_line.options) {
        return command_line.exit_code;
    }

    auto options{*command_line.options};
    if (options.fighter_stress_caps.empty()) {
        options.fighter_stress_caps.push_back(options.fighter_stress_cap.value_or(0));
    }

    for (auto const cap : options.fighter_stress_caps) {
        options.fighter_stress_cap = cap > 0 ? std::optional{cap} : std::nullopt;
        auto result{ml::simulation_benchmark::run_benchmark(options, [] {
            std::cerr << ml::simulation_benchmark::profiler_ready_message << '\n' << std::flush;
        })};
        if (!result) {
            std::cerr << result.error() << '\n';
            return 1;
        }

        std::cout << ml::simulation_benchmark::to_json(*result) << '\n';
        options.profiler_connection_timeout_seconds.reset();
    }
    return 0;
}
