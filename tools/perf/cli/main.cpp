#include <sandbox/perf/benchmark_comparison.hpp>

#include <iostream>
#include <string_view>

auto main(int const argc, char** argv) -> int {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "tracy-benchmark-compare " << IOJ_PERF_VERSION << '\n';
        return 0;
    }
    return sandbox::perf::run_application(argc, argv, std::cout, std::cerr);
}
