#pragma once

#include "cli_args.h"
#include <ioj/host/mission_startup.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace ioj::sandbox_host {
inline constexpr ScreenResolution startup_size{1280, 720};

using Result = std::expected<void, std::string>;
using PathResult = std::expected<std::filesystem::path, std::string>;

auto preferences_file() -> PathResult;

struct LaunchPaths {
    std::filesystem::path binaries;
    std::filesystem::path scripts;
    std::filesystem::path personal;
    std::filesystem::path mission;
};

auto engine_arguments(std::filesystem::path const& directory,
                      LaunchDisplayMode display,
                      std::filesystem::path const& user_directory) -> std::wstring;
auto initial_display(CliArgs const& args, WindowDisplaySettings saved)
    -> std::optional<WindowDisplaySettings>;

struct NativeEngine;

class Engine {
  public:
    Engine();
    ~Engine();
    Engine(Engine const&) = delete;
    auto operator=(Engine const&) -> Engine& = delete;

    auto start(LaunchPaths const& paths, CliArgs const& args) -> Result;
    auto play(LaunchDisplayMode mode, std::filesystem::path const& mission, bool exit_when_ready)
        -> Result;
    auto shutdown() -> int;

  private:
    auto mission_startup() const -> MissionStartup;
    auto world_ready() const -> bool;
    auto show_window() -> Result;
    auto launch_mission(std::filesystem::path const& mission) -> bool;
    auto exit_requested() const -> bool;
    void pump_messages();
    auto synchronize_window() -> Result;
    auto tick() -> Result;

    std::unique_ptr<NativeEngine> native_;
};
}
