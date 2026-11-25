#pragma once

#include <ioj/settings/launch_display_mode.h>
#include <ioj/settings/screen_resolution.h>

#include <expected>
#include <filesystem>
#include <optional>

namespace ioj::sandbox_host {
struct CliArgs {
    std::filesystem::path mission;
    std::filesystem::path editor_scripts;
    std::filesystem::path user_directory;
    LaunchDisplayMode display{LaunchDisplayMode::Preferences};
    std::optional<ScreenResolution> size;
    bool exit_when_ready{};
};

using CliResult = std::expected<CliArgs, int>;

// Print CLI11 help or diagnostics and return the associated exit status on early exit.
auto parse_cli_args(int argc, char** argv) -> CliResult;
}
