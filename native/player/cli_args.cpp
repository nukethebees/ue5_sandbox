#include "cli_args.h"

#include <ioj/settings/launch_display_mode.h>
#include <ioj/settings/screen_resolution.h>

#include <CLI/CLI.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace ioj::player {
namespace {
auto dimension(std::string_view text, std::int32_t& value) -> bool {
    auto const end{text.data() + text.size()};
    auto const result{std::from_chars(text.data(), end, value)};
    return result.ec == std::errc{} && result.ptr == end && value > 0;
}

auto parse_resolution(std::string_view text) -> std::optional<ioj::ScreenResolution> {
    auto const separator{text.find('x')};
    ioj::ScreenResolution size{};
    if (separator == text.npos || !dimension(text.substr(0, separator), size.width) ||
        !dimension(text.substr(separator + 1), size.height)) {
        return {};
    }

    return size;
}
}

// Parse UTF-8 arguments before starting the engine.
auto parse_cli_args(int argc, char** argv) -> CliResult {
    CLI::App app{"Isle of Jebbers player"};
    argv = app.ensure_utf8(argv);

    CliArgs args{};
    std::map<std::string, LaunchDisplayMode> const modes{
        {"windowed", LaunchDisplayMode::Windowed},
        {"borderless", LaunchDisplayMode::Borderless},
        {"headless", LaunchDisplayMode::Headless},
    };
    auto set_mode = [&](std::string const& name) { args.launch_display_mode = modes.at(name); };
    app.add_option_function<std::string>(
           "--display-mode", set_mode, "Override the saved display mode")
        ->check(CLI::IsMember(modes));

    auto set_size = [&](std::string const& text) {
        args.initial_window_size = parse_resolution(text);
        if (!args.initial_window_size) {
            throw CLI::ValidationError("--window-size", "expected positive WIDTHxHEIGHT in pixels");
        }
    };
    app.add_option_function<std::string>(
           "--window-size", set_size, "Initial client size for windowed mode")
        ->type_name("WIDTHxHEIGHT");

    app.add_flag("--exit-when-ready",
                 args.exit_when_ready,
                 "Exit after the menu or mission is ready (headless by default)");

    app.add_option(
        "--mission", args.mission_path, "S7 mission (absolute or relative to the staged project)");
    app.add_option("--editor-scripts",
                   args.editor_scripts_directory,
                   "Read settings and preferences from this LevelScripts directory")
        ->check(CLI::ExistingDirectory);
    app.add_option("--user-dir",
                   args.user_directory,
                   "Use this directory for Unreal saves and personal preferences");

    try {
        app.parse(argc, argv);
        if (args.initial_window_size && args.launch_display_mode != LaunchDisplayMode::Windowed) {
            throw CLI::ValidationError("--window-size", "requires --display-mode windowed");
        }
    } catch (CLI::ParseError const& error) {
        return CliResult{std::unexpect, app.exit(error)};
    }

    if (args.exit_when_ready && args.launch_display_mode == LaunchDisplayMode::Preferences) {
        args.launch_display_mode = LaunchDisplayMode::Headless;
    }

    return args;
}
}
