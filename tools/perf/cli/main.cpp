#include <sandbox/perf/benchmark_comparison.hpp>

#include <iostream>

auto main(int const argc, char** argv) -> int {
    return sandbox::perf::run_application(argc, argv, std::cout, std::cerr);
}
