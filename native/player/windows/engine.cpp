#include "player/engine.h"

#include "player/cli_args.h"
#include "player/window.h"
#include "player/windows/system.h"
#include "player/windows/unreal_library.h"
#include "player/windows/window.h"
#include <ioj/host/game_host_api.h>

#include <expected>
#include <format>
#include <string>

namespace ioj {
auto start_engine(Window& window, LaunchPaths const& paths, CliArgs const& args) -> ApplicationResult {
    auto const arguments{engine_arguments(paths.binary_directory, args)};
    auto const status{UELibrary_InitW(
        GetModuleHandleW(nullptr), native_handle(window.sdl_window()), arguments.c_str())};
    if (status != 0) {
        IojPlayer_Detach();
        return ApplicationResult{std::unexpect, std::format("UELibrary_InitW failed ({})", status)};
    }

    if (args.launch_display_mode != LaunchDisplayMode::Headless) {
        if (auto result{connect_unreal_window(window.sdl_window())}; !result) {
            shutdown_engine(window);
            return result;
        }
    }
    return {};
}

auto shutdown_engine(Window& window) -> ApplicationResult {
    disconnect_unreal_window(window.sdl_window());
    auto const failed{IojPlayer_GameFailed()};
    auto const status{UELibrary_Shutdown()};
    IojPlayer_Detach();
    if (status != 0 || failed) {
        return std::unexpected{
            std::format("Game exited with an error ({})", status != 0 ? status : 1)};
    }
    return {};
}

auto tick_engine() -> ApplicationResult {
    if (auto status{UELibrary_Tick()}; status != 0) {
        return ApplicationResult{std::unexpect, std::format("UELibrary_Tick failed ({})", status)};
    }

    return {};
}
}
