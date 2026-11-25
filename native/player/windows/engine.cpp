#include "../engine.h"

#include "../cli_args.h"
#include "../window.h"
#include "system.h"
#include "unreal_library.h"
#include "window.h"
#include <ioj/host/game_host_api.h>

#include <expected>
#include <format>
#include <string>

namespace ioj::player {
auto start_engine(Window& window, LaunchPaths const& paths, CliArgs const& args) -> Result {
    auto const arguments{engine_arguments(paths.binary_directory, args)};
    auto const status{UELibrary_InitW(
        GetModuleHandleW(nullptr), native_handle(window.sdl_window()), arguments.c_str())};
    if (status != 0) {
        IojPlayer_Detach();
        return Result{std::unexpect, std::format("UELibrary_InitW failed ({})", status)};
    }

    if (args.launch_display_mode != LaunchDisplayMode::Headless) {
        if (auto result{connect_unreal_window(window.sdl_window())}; !result) {
            shutdown_engine(window);
            return result;
        }
    }
    return {};
}

auto shutdown_engine(Window& window) -> Result {
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

auto tick_engine() -> Result {
    if (auto status{UELibrary_Tick()}; status != 0) {
        return Result{std::unexpect, std::format("UELibrary_Tick failed ({})", status)};
    }

    return {};
}
}
