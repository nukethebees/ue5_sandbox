#pragma once

#include <ioj/settings/launch_display_mode.h>
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

auto engine_arguments(std::filesystem::path const& project_directory,
                      LaunchDisplayMode mode,
                      std::filesystem::path const& user_directory) -> std::wstring;
auto initial_display(LaunchDisplayMode mode,
                     std::optional<ScreenResolution> window_size,
                     WindowDisplaySettings saved) -> std::optional<WindowDisplaySettings>;

auto start_engine(Window& window, std::wstring const& arguments, LaunchDisplayMode mode)
    -> ApplicationResult;

auto shutdown_engine(Window& window) -> ApplicationResult;
auto tick_engine() -> ApplicationResult;
}
