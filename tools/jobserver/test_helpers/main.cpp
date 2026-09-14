#include <Windows.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

auto main(int argc, char** argv) -> int {
    if (argc < 2) {
        return 2;
    }
    auto const mode{std::string{argv[1]}};
    if (mode == "exit") {
        return argc >= 3 ? std::atoi(argv[2]) : 0;
    }
    if (mode == "sleep") {
        auto const milliseconds{argc >= 3 ? std::atoi(argv[2]) : 100};
        std::this_thread::sleep_for(std::chrono::milliseconds{milliseconds});
        return 0;
    }
    if (mode == "output") {
        auto const count{argc >= 3 ? std::atoi(argv[2]) : 3};
        for (auto index{0}; index != count; ++index) {
            std::cout << "stdout " << index << '\n' << std::flush;
            std::cerr << "stderr " << index << '\n' << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }
        return 0;
    }
    if (mode == "crash") {
        std::abort();
    }
    if (mode == "spawn") {
        STARTUPINFOW startup{};
        startup.cb = sizeof(STARTUPINFOW);
        PROCESS_INFORMATION child{};
        std::wstring command{L"jobserver-test-helper.exe sleep 60000"};
        if (!CreateProcessW(nullptr,
                            command.data(),
                            nullptr,
                            nullptr,
                            FALSE,
                            0,
                            nullptr,
                            nullptr,
                            &startup,
                            &child)) {
            return 3;
        }
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
        return 0;
    }
    return 2;
}
