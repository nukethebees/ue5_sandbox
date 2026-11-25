#pragma once

#include "cli_args.h"
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace ioj::player {
class Window;

inline constexpr ScreenResolution startup_size{1280, 720};

using Result = std::expected<void, std::string>;
using PathResult = std::expected<std::filesystem::path, std::string>;

auto local_app_data() -> PathResult;

struct LaunchPaths {
    std::filesystem::path binary_directory;
    std::filesystem::path scripts_directory;
    std::filesystem::path user_settings_path;
    std::filesystem::path mission_path;
    std::filesystem::path save_directory;
};

auto engine_arguments(std::filesystem::path const& directory, CliArgs const& args) -> std::wstring;
auto initial_display(CliArgs const& args, WindowDisplaySettings saved)
    -> std::optional<WindowDisplaySettings>;

auto start_engine(Window& window, LaunchPaths const& paths, CliArgs const& args) -> Result;

auto shutdown_engine(Window& window) -> Result;
auto tick_engine() -> Result;
}
