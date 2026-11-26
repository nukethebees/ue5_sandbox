#pragma once

#include "player/cli_args.h"
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/window_display_settings.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace ioj {
class Window;

inline constexpr ScreenResolution startup_size{1280, 720};

using ApplicationResult = std::expected<void, std::string>;
using FilesystemPathResult = std::expected<std::filesystem::path, std::string>;

auto local_app_data() -> FilesystemPathResult;

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

auto start_engine(Window& window, LaunchPaths const& paths, CliArgs const& args) -> ApplicationResult;

auto shutdown_engine(Window& window) -> ApplicationResult;
auto tick_engine() -> ApplicationResult;
}
