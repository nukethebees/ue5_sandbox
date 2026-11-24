#include "application.h"

#include "cli_args.h"
#include <ioj/files.h>
#include <ioj/host/game_host.h>

#include "engine.h"

#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <utility>

namespace ioj::sandbox_host {
namespace {
auto personal_settings_path(CliArgs const& args, std::filesystem::path const& scripts)
    -> PathResult {
    if (!args.user_directory.empty()) {
        return std::filesystem::absolute(args.user_directory) / "Config/user-settings.scm";
    }

    if (!args.editor_scripts.empty()) {
        return scripts / "Settings/user-settings.scm";
    }

    return preferences_file();
}

using PathsResult = std::expected<LaunchPaths, std::string>;

auto resolve_launch_paths(CliArgs const& args) -> PathsResult {
    auto directory{executable_directory()};
    if (!directory) {
        return PathsResult{std::unexpect, std::move(directory.error())};
    }

    auto const project{directory->parent_path().parent_path()};
    std::filesystem::path mission;
    if (!args.mission.empty()) {
        mission = (project / args.mission).lexically_normal();
        if (!std::filesystem::is_regular_file(mission)) {
            return PathsResult{std::unexpect,
                               "Mission file does not exist: " + path_to_utf8(mission)};
        }
    }

    auto scripts{args.editor_scripts.empty() ? project / "LevelScripts"
                                             : std::filesystem::absolute(args.editor_scripts)};
    auto personal{personal_settings_path(args, scripts)};
    if (!personal) {
        return PathsResult{std::unexpect, std::move(personal.error())};
    }

    return PathsResult{std::in_place,
                       std::move(*directory),
                       std::move(scripts),
                       std::move(*personal),
                       std::move(mission)};
}

}

Application::Application(CliArgs args)
    : args_{std::move(args)} {}

auto Application::run() const -> Result {
    auto paths{resolve_launch_paths(args_)};
    if (!paths) {
        return Result{std::unexpect, std::move(paths.error())};
    }

    GameHost host{.launch_display = args_.display};
    Engine engine;
    if (auto result{engine.start(*paths, args_, host)}; !result) {
        return result;
    }

    auto result{engine.play(host, paths->mission, args_.exit_when_ready)};
    auto const status{engine.shutdown()};
    if (!result) {
        return result;
    }
    if (status != 0) {
        return Result{std::unexpect, std::format("UELibrary_Shutdown failed ({})", status)};
    }

    return {};
}
}
