#include "../windows/system.h"
#include <ioj/host/game_host_api.h>
#include <ioj/host/mission_startup.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>
#include <ioj/settings/windows_display.h>

#include <climits>
#include <cstdint>
#include <cstring>
#include <print>
#include <string_view>

namespace ioj {
namespace {

HWND window{};
bool done{};

auto scenario() -> std::string_view {
    static char name[32]{};
    GetEnvironmentVariableA("SANDBOX_FIXTURE_CASE", name, sizeof(name));
    return name;
}
}
auto SandboxHost_MissionStartup() -> MissionStartup {
    return MissionStartup::Ready;
}
void SandboxHost_SetInitialDisplay(WindowDisplaySettings) {}
auto SandboxHost_Prepare(LaunchDisplayMode,
                         wchar_t const*,
                         wchar_t const*,
                         ScreenResolution desktop,
                         WindowDisplaySettings* saved) -> char const* {
    std::println("fixture:prepare");
    if (scenario() == "prepare") {
        return "fixture settings failed";
    }
    *saved = {GameWindowMode::Windowed, desktop};
    return nullptr;
}
void SandboxHost_Detach() {
    std::println("fixture:detach");
}
auto SandboxHost_GameReady() -> bool {
    std::println("fixture:ready");
    return scenario() != "readiness";
}
auto SandboxHost_ExitRequested() -> bool {
    return done;
}
void SandboxHost_PumpMessages() {}
auto SandboxHost_InitializeWindow(int, int, int width, int height, bool) -> bool {
    std::println("fixture:client {}x{}", width, height);
    return true;
}
auto SandboxHost_LaunchMission(wchar_t const*) -> bool {
    return true;
}

extern "C" {
    __declspec(dllexport) auto UELibrary_InitW(HINSTANCE, HWND hwnd, wchar_t const*) -> int {
        std::println("fixture:init");
        window = hwnd;
        SendMessageW(window, WM_NULL, 0, 0);
        return scenario() == "init" ? 7 : 0;
    }
    __declspec(dllexport) auto UELibrary_WndProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
        -> LRESULT {
        if (message == WM_NULL) {
            std::println("fixture:forward");
        }
        return DefWindowProcW(hwnd, message, wp, lp);
    }
    __declspec(dllexport) auto UELibrary_Tick() -> int {
        std::println("fixture:tick");
        if (scenario() == "display") {
            SendMessageW(window, WM_NULL, 0, 0);
            WindowDisplaySettings display{GameWindowMode::Windowed, {1500, 850}};
            WindowsDisplay native{window};
            if (!native.preview_display(display) || !native.verify_preview() || !native.restore()) {
                return 1;
            }
            display.resolution = {INT_MAX, INT_MAX};
            if (native.preview_display(display)) {
                return 2;
            }
            SendMessageW(window, WM_CLOSE, 0, 0);
        }
        done = true;
        return scenario() == "tick" ? 8 : 0;
    }
    __declspec(dllexport) auto UELibrary_Shutdown() -> int {
        std::println("fixture:shutdown");
        SendMessageW(window, WM_NULL, 0, 0);
        DestroyWindow(window);
        return 0;
    }
}
}
