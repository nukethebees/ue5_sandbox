#include "server.hpp"

#include <Windows.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace jobserver::daemon_logging {
inline constexpr std::uintmax_t maximum_log_size{1024U * 1024U};

auto data_directory() -> std::filesystem::path {
    char* test_data{};
    std::size_t test_data_size{};
    if (_dupenv_s(&test_data, &test_data_size, "NUKETHEBEES_JOBSERVER_TEST_DATA") == 0 &&
        test_data != nullptr) {
        std::filesystem::path const result{test_data};
        std::free(test_data);
        return result;
    }

    char* local_app_data{};
    std::size_t size{};
    if (_dupenv_s(&local_app_data, &size, "LOCALAPPDATA") != 0 || local_app_data == nullptr) {
        return {};
    }
    std::filesystem::path const result{std::filesystem::path{local_app_data} / "NukeTheBees" /
                                       "jobserver" / "data"};
    std::free(local_app_data);
    return result;
}

auto open() -> std::ofstream {
    auto const directory{data_directory()};
    if (directory.empty()) {
        return {};
    }
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return {};
    }

    auto const current{directory / "jobserverd.log"};
    auto const previous{directory / "jobserverd.previous.log"};
    auto const size{std::filesystem::file_size(current, error)};
    if (!error && size >= maximum_log_size) {
        std::filesystem::remove(previous, error);
        error.clear();
        std::filesystem::rename(current, previous, error);
    }
    return std::ofstream{current, std::ios::app};
}
}

auto WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) -> int {
    auto log{jobserver::daemon_logging::open()};
    auto* original_stderr{std::cerr.rdbuf()};
    if (log) {
        std::cerr.rdbuf(log.rdbuf());
    }
    std::cerr << "Jobserver daemon starting (pid " << GetCurrentProcessId() << ")\n";
    jobserver::Server server;
    auto const result{server.run()};
    std::cerr << "Jobserver daemon exiting with code " << result << '\n';
    std::cerr.flush();
    std::cerr.rdbuf(original_stderr);
    return result;
}
