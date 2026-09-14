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

    auto result{ml::simulation_benchmark::run_benchmark(*command_line.options, [] {
        std::cerr << ml::simulation_benchmark::profiler_ready_message << '\n' << std::flush;
    })};
    if (!result) {
        std::cerr << result.error() << '\n';
        return 1;
    }

    std::cout << ml::simulation_benchmark::to_json(*result) << '\n';
    return 0;
}
