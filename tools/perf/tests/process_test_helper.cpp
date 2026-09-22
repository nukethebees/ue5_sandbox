#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <thread>

auto main(int const argc, char** argv) -> int {
    auto const executable{std::filesystem::path{argv[0]}.filename()};
    if (executable == "tracy-capture.exe") {
        for (auto index{1}; index + 1 < argc; ++index) {
            if (std::string_view{argv[index]} == "-o") {
                std::ofstream{std::string{argv[index + 1]} + ".started"} << "started\n";
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds{30});
        return 0;
    }
    for (auto index{1}; index < argc; ++index) {
        if (std::string_view{argv[index]} == "ready-and-block") {
            std::cerr << "native-simulation-benchmark: profiler-ready\n" << std::flush;
            std::this_thread::sleep_for(std::chrono::seconds{30});
            return 0;
        }
    }
    if (argc > 1 && std::string_view{argv[1]} == "ready") {
        std::cerr << "native-simulation-benchmark: profiler-ready\n" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds{30});
        return 0;
    }
    return argc > 2 && std::string_view{argv[1]} == "exit" ? std::stoi(argv[2]) : 1;
}
