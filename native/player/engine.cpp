#include "player/engine.h"

#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/launch_display_mode.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>
#include <utility>

namespace ioj {
auto engine_arguments(std::filesystem::path const& project_directory,
                      LaunchDisplayMode mode,
                      std::filesystem::path const& user_directory) -> std::wstring {
    auto const project{project_directory / L"Sandbox.uproject"};
    auto arguments{
        std::format(L"\"{}\" /SpaceGame/Levels/MainMenu -game -windowed", project.native())};

    if (mode == LaunchDisplayMode::Headless) {
        arguments += L" -RenderOffScreen -nullrhi -nosound -nosplash -unattended";
    } else {
        arguments += L" -UELibraryWindowActivation";
    }

    if (!user_directory.empty()) {
        arguments += std::format(L" -UserDir=\"{}\"", user_directory.native());
    }

    return arguments;
}

auto initial_display(LaunchDisplayMode mode,
                     std::optional<ScreenResolution> window_size,
                     WindowDisplaySettings saved) -> std::optional<WindowDisplaySettings> {
    switch (mode) {
        case LaunchDisplayMode::Headless:
            return std::nullopt;
        case LaunchDisplayMode::Windowed: {
            ScreenResolution const capped{std::min(saved.resolution.width, startup_size.width),
                                          std::min(saved.resolution.height, startup_size.height)};
            return WindowDisplaySettings{GameWindowMode::Windowed, window_size.value_or(capped)};
        }
        case LaunchDisplayMode::Borderless:
            return WindowDisplaySettings{GameWindowMode::Borderless, {}};
        case LaunchDisplayMode::Preferences:
            return saved;
    }

    std::unreachable();
}

}
