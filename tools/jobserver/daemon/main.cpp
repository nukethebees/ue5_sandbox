#include "server.hpp"

#include "jobserver/transport.hpp"

#include <Windows.h>

#include <cstdlib>

auto WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) -> int {
    wchar_t* app_data{};
    std::size_t size{};
    if (_wdupenv_s(&app_data, &size, L"LOCALAPPDATA") != 0 || !app_data) {
        return 1;
    }
    auto const codex{std::filesystem::path{app_data} / "NukeTheBees" / "agent-codex" / "bin" /
                     "codex-scheduler.exe"};
    std::free(app_data);
    jobserver::Server server{jobserver::transport::pipe_name(), codex};
    return server.run();
}
