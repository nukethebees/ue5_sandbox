#pragma once

#include "../engine.h"
#include "system.h"
#include <ioj/host/game_host_api.h>

#include <filesystem>

namespace ioj::sandbox_host {
using MessageHandler = LRESULT (*)(HWND, UINT, WPARAM, LPARAM);

struct Library {
    auto load(std::filesystem::path const& path) -> Result;

    int (*init)(HINSTANCE, HWND, wchar_t const*){};
    int (*tick)(){};
    int (*shutdown)(){};
    MessageHandler wnd_proc{};

    decltype(&SandboxHost_Prepare) prepare{};
    decltype(&SandboxHost_Detach) detach{};
    decltype(&SandboxHost_GameReady) ready{};
    decltype(&SandboxHost_ExitRequested) wants_exit{};
    decltype(&SandboxHost_PumpMessages) pump{};
    decltype(&SandboxHost_InitializeWindow) sync{};
    decltype(&SandboxHost_LaunchMission) launch{};
};
}
