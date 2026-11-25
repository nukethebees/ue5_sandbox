#pragma once

#include <ioj/settings/launch_display_mode.h>
#include <ioj/settings/screen_resolution.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace ioj::player {
struct CliArgs {
    std::filesystem::path mission_path;
    std::filesystem::path editor_scripts_directory;
    std::filesystem::path user_directory;
    LaunchDisplayMode launch_display_mode{LaunchDisplayMode::Preferences};
    std::optional<ScreenResolution> initial_window_size;
    bool exit_when_ready{};
};

using CliResult = std::expected<CliArgs, int>;

// Print CLI11 help or diagnostics and return the associated exit status on early exit.
auto parse_cli_args(int argc, char** argv) -> CliResult;
}
