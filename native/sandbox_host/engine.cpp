#include "engine.h"

#include "cli_args.h"
#include <ioj/files.h>
#include <ioj/host/game_host.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/launch_display.h>
#include <ioj/settings/screen_resolution.h>

#include <algorithm>
#include <chrono>
#include <expected>
#include <filesystem>
#include <optional>
#include <print>
#include <utility>

namespace ioj::sandbox_host {
auto initial_display(CliArgs const& args, HostDisplay saved) -> std::optional<HostDisplay> {
    switch (args.display) {
        case LaunchDisplay::Headless:
            return std::nullopt;
        case LaunchDisplay::Windowed: {
            ScreenResolution const capped{std::min(saved.resolution.width, startup_size.width),
                                          std::min(saved.resolution.height, startup_size.height)};
            return HostDisplay{GameWindowMode::Windowed, args.size.value_or(capped)};
        }
        case LaunchDisplay::Borderless:
            return HostDisplay{GameWindowMode::Borderless, {}};
        case LaunchDisplay::Preferences:
            return saved;
    }
    std::unreachable();
}

auto Engine::play(GameHost const& host, std::filesystem::path const& mission, bool exit_when_ready)
    -> Result {
    if (!world_ready()) {
        return Result{std::unexpect,
                      "Sandbox did not initialize a playable world. See the game log."};
    }

    if (host.launch_display != LaunchDisplay::Headless) {
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
            if (mission.empty() || host.mission_startup == MissionStartup::Ready) {
                std::println("Ready: {}", mission.empty() ? "MainMenu" : path_to_utf8(mission));
                return {};
            }

            if (host.mission_startup == MissionStartup::Failed ||
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
