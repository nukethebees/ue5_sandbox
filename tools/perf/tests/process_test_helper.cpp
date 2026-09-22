#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

auto main(int const argc, char** argv) -> int {
    if (argc > 1 && std::string_view{argv[1]} == "ready") {
        std::cerr << "native-simulation-benchmark: profiler-ready\n" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds{30});
        return 0;
    }
    return argc > 2 && std::string_view{argv[1]} == "exit" ? std::stoi(argv[2]) : 1;
}
