#include "../windows/system.h"
#include "SandboxHostFingerprint.h"
#include <ioj/host/game_host.h>
#include <ioj/host/game_host_api.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/launch_display.h>
#include <ioj/settings/screen_resolution.h>

#include <climits>
#include <cstdint>
#include <cstring>
#include <print>
#include <string_view>

namespace ioj {
namespace {
GameHost* host{};
HWND window{};
bool done{};

auto scenario() -> std::string_view {
    static char name[32]{};
    GetEnvironmentVariableA("SANDBOX_FIXTURE_CASE", name, sizeof(name));
    return name;
}
}
auto SandboxHost_Compatible(std::uint32_t version, std::uint64_t layout, char const* fingerprint)
    -> bool {
    std::println("fixture:compatible");
    if (version != host_interface_version || layout != host_layout_signature() ||
        scenario() == "version") {
        return false;
    }
    if (std::strcmp(fingerprint, SANDBOX_HOST_FINGERPRINT) != 0) {
        std::println("fixture:same-size-mismatch");
        return false;
    }
    return true;
}
auto SandboxHost_Prepare(GameHost* application,
                         wchar_t const*,
                         wchar_t const*,
                         ScreenResolution desktop,
                         HostDisplay* saved) -> char const* {
    std::println("fixture:prepare");
    if (scenario() == "prepare") {
        return "fixture settings failed";
    }
    host = application;
    *saved = {GameWindowMode::Windowed, desktop};
    return nullptr;
}
void SandboxHost_Detach() {
    std::println("fixture:detach");
    host = nullptr;
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
            HostDisplay display{GameWindowMode::Windowed, {1500, 850}};
            if (host->apply_display(host->window, DisplayOperation::Preview, display) ||
                host->apply_display(host->window, DisplayOperation::Verify, display) ||
                host->apply_display(host->window, DisplayOperation::Revert, display)) {
                return 1;
            }
            display.resolution = {INT_MAX, INT_MAX};
            if (!host->apply_display(host->window, DisplayOperation::Preview, display)) {
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
