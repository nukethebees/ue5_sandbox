#include "player/engine.h"

#include "player/cli_args.h"
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/launch_display_mode.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>

#include <algorithm>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <utility>

namespace ioj {
auto engine_arguments(std::filesystem::path const& directory, CliArgs const& args) -> std::wstring {
    auto const project{directory.parent_path().parent_path() / L"Sandbox.uproject"};
    auto arguments{
        std::format(L"\"{}\" /SpaceGame/Levels/MainMenu -game -windowed", project.native())};

    if (args.launch_display_mode == LaunchDisplayMode::Headless) {
        arguments += L" -RenderOffScreen -nullrhi -nosound -nosplash -unattended";
    } else {
        arguments += L" -UELibraryWindowActivation";
    }

    if (!args.user_directory.empty()) {
        arguments += std::format(L" -UserDir=\"{}\"",
                                 std::filesystem::absolute(args.user_directory).native());
    }

    return arguments;
}

auto initial_display(CliArgs const& args, WindowDisplaySettings saved)
    -> std::optional<WindowDisplaySettings> {
    switch (args.launch_display_mode) {
        case LaunchDisplayMode::Headless:
            return std::nullopt;
        case LaunchDisplayMode::Windowed: {
            ScreenResolution const capped{std::min(saved.resolution.width, startup_size.width),
                                          std::min(saved.resolution.height, startup_size.height)};
            return WindowDisplaySettings{GameWindowMode::Windowed,
                                         args.initial_window_size.value_or(capped)};
        }
        case LaunchDisplayMode::Borderless:
            return WindowDisplaySettings{GameWindowMode::Borderless, {}};
        case LaunchDisplayMode::Preferences:
            return saved;
    }

    std::unreachable();
}

}
