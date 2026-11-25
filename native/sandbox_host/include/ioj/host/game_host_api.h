#pragma once

#include <ioj/host/mission_startup.h>
#include <ioj/settings/launch_display_mode.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>

namespace ioj {
#ifdef SANDBOX_HOST_EXPORTS
#define SANDBOX_HOST_API __declspec(dllexport)
#else
#define SANDBOX_HOST_API
#endif
extern "C" {
    // The host borrows the diagnostic until the next prepare. Detach after a successful prepare.
    SANDBOX_HOST_API auto SandboxHost_Prepare(LaunchDisplayMode mode,
                                              wchar_t const* scripts,
                                              wchar_t const* personal,
                                              ScreenResolution desktop,
                                              WindowDisplaySettings* saved) -> char const*;
    SANDBOX_HOST_API void SandboxHost_SetInitialDisplay(WindowDisplaySettings actual);
    SANDBOX_HOST_API auto SandboxHost_MissionStartup() -> MissionStartup;
    SANDBOX_HOST_API void SandboxHost_Detach();
    SANDBOX_HOST_API auto SandboxHost_GameReady() -> bool;
    SANDBOX_HOST_API auto SandboxHost_ExitRequested() -> bool;
    SANDBOX_HOST_API void SandboxHost_PumpMessages();
    SANDBOX_HOST_API auto
        SandboxHost_InitializeWindow(int x, int y, int width, int height, bool activate) -> bool;
    SANDBOX_HOST_API auto SandboxHost_LaunchMission(wchar_t const* path) -> bool;
}
#undef SANDBOX_HOST_API
}
