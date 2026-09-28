#include "server.hpp"

#include <Windows.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace jobserver::daemon_logging {

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

}
auto WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) -> int {
    jobserver::Server server{jobserver::daemon_logging::data_directory()};
    return server.run();
}
