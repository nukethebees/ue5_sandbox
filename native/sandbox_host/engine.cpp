#include "engine.h"

#include "cli_args.h"
#include <ioj/files.h>
#include <ioj/host/mission_startup.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/launch_display_mode.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>

#include <algorithm>
#include <chrono>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <utility>

namespace ioj::sandbox_host {
auto engine_arguments(std::filesystem::path const& directory,
                      LaunchDisplayMode display,
                      std::filesystem::path const& user_directory) -> std::wstring {
    auto const project{directory.parent_path().parent_path() / L"Sandbox.uproject"};
    auto arguments{
        std::format(L"\"{}\" /SpaceGame/Levels/MainMenu -game -windowed", project.native())};

    if (display == LaunchDisplayMode::Headless) {
        arguments += L" -RenderOffScreen -nullrhi -nosound -nosplash -unattended";
    } else {
        arguments += L" -UELibraryWindowActivation";
    }

    if (!user_directory.empty()) {
        arguments += std::format(L" -SpaceSaveLocal -UserDir=\"{}\"",
                                 std::filesystem::absolute(user_directory).native());
    }

    return arguments;
}

auto initial_display(CliArgs const& args, WindowDisplaySettings saved)
    -> std::optional<WindowDisplaySettings> {
    switch (args.display) {
        case LaunchDisplayMode::Headless:
            return std::nullopt;
        case LaunchDisplayMode::Windowed: {
            ScreenResolution const capped{std::min(saved.resolution.width, startup_size.width),
                                          std::min(saved.resolution.height, startup_size.height)};
            return WindowDisplaySettings{GameWindowMode::Windowed, args.size.value_or(capped)};
        }
        case LaunchDisplayMode::Borderless:
            return WindowDisplaySettings{GameWindowMode::Borderless, {}};
        case LaunchDisplayMode::Preferences:
            return saved;
    }
    std::unreachable();
}

auto Engine::play(LaunchDisplayMode mode,
                  std::filesystem::path const& mission,
                  bool exit_when_ready) -> Result {
    if (!world_ready()) {
        return Result{std::unexpect,
                      "Sandbox did not initialize a playable world. See the game log."};
    }

    if (mode != LaunchDisplayMode::Headless) {
        if (auto result{show_window()}; !result) {
            return result;
        }
    }

    if (!mission.empty() && !launch_mission(mission)) {
        return Result{std::unexpect, "Mission launch failed: " + path_to_utf8(mission)};
    }

    auto const deadline{std::chrono::steady_clock::now() + std::chrono::minutes{2}};
    while (!exit_requested()) {
        pump_messages();
        if (exit_requested()) {
            break;
        }

        if (auto result{synchronize_window()}; !result) {
            return result;
        }

        if (exit_when_ready) {
            if (mission.empty() || mission_startup() == MissionStartup::Ready) {
                std::println("Ready: {}", mission.empty() ? "MainMenu" : path_to_utf8(mission));
                return {};
            }

            if (mission_startup() == MissionStartup::Failed ||
                std::chrono::steady_clock::now() >= deadline) {
                return Result{std::unexpect,
                              "Mission did not reach simulation readiness. See the game log."};
            }
        }

        if (auto result{tick()}; !result) {
            return result;
        }
    }

    if (exit_when_ready) {
        return Result{std::unexpect, "Engine exited before readiness"};
    }
    return {};
}
}
