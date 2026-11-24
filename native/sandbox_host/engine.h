#pragma once

#include "cli_args.h"
#include <ioj/host/game_host.h>
#include <ioj/settings/launch_display.h>
#include <ioj/settings/screen_resolution.h>

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace ioj::sandbox_host {
inline constexpr ScreenResolution startup_size{1280, 720};

using Result = std::expected<void, std::string>;
using PathResult = std::expected<std::filesystem::path, std::string>;

auto executable_directory() -> PathResult;
auto preferences_file() -> PathResult;
auto desktop_resolution() -> std::expected<ScreenResolution, std::string>;

struct LaunchPaths {
    std::filesystem::path binaries;
    std::filesystem::path scripts;
    std::filesystem::path personal;
    std::filesystem::path mission;
};

auto initial_display(CliArgs const& args, HostDisplay saved) -> std::optional<HostDisplay>;

struct NativeEngine;

class Engine {
  public:
    Engine();
    ~Engine();
    Engine(Engine const&) = delete;
    auto operator=(Engine const&) -> Engine& = delete;

    auto start(LaunchPaths const& paths, CliArgs const& args, GameHost& host) -> Result;
    auto play(GameHost const& host, std::filesystem::path const& mission, bool exit_when_ready)
        -> Result;
    auto shutdown() -> int;

  private:
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
