#include "windows/library.h"

#include "build_identity.h"
#include "SandboxGameBuildId.h"
#include <ioj/files.h>
#include <ioj/host/game_host_api.h>

#include "engine.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_loadso.h>

#include <expected>
#include <filesystem>
#include <string>
#include <type_traits>
#include <utility>

namespace ioj::sandbox_host {
auto Library::load(std::filesystem::path const& path) -> Result {
    if (auto result{verify_build_identity(path, SANDBOX_GAME_BUILD_ID)}; !result) {
        return result;
    }

    // Unreal stays loaded until process exit, including after UELibrary_Shutdown.
    auto const module{SDL_LoadObject(path_to_utf8(path).c_str())};
    if (!module) {
        return std::unexpected{std::string{SDL_GetError()}};
    }

    std::string error;
    auto bind = [&](auto& function, char const* name) {
        if (!error.empty()) {
            return;
        }

        auto const address{SDL_LoadFunction(module, name)};
        if (!address) {
            error = SDL_GetError();
            return;
        }

        function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(address);
    };

    bind(init, "UELibrary_InitW");
    bind(tick, "UELibrary_Tick");
    bind(shutdown, "UELibrary_Shutdown");
    bind(wnd_proc, "UELibrary_WndProc");

    bind(prepare, "SandboxHost_Prepare");
    bind(mission_startup, "SandboxHost_MissionStartup");
    bind(initial_display, "SandboxHost_SetInitialDisplay");
    bind(detach, "SandboxHost_Detach");
    bind(ready, "SandboxHost_GameReady");
    bind(wants_exit, "SandboxHost_ExitRequested");
    bind(pump, "SandboxHost_PumpMessages");
    bind(sync, "SandboxHost_InitializeWindow");
    bind(launch, "SandboxHost_LaunchMission");

    if (!error.empty()) {
        return Result{std::unexpect, std::move(error)};
    }
    return {};
}
} // namespace ioj::sandbox_host
