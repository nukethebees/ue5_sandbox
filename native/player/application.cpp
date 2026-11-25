#include "application.h"

#include "cli_args.h"
#include "engine.h"
#include "window.h"
#include <ioj/files.h>
#include <ioj/host/game_host_api.h>
#include <ioj/profiles/file_profile_storage.h>


#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>

#include <cstddef>
#include <cstdio>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <print>
#include <span>
#include <string>
#include <utility>

namespace ioj::player {
namespace {
auto user_settings_path(CliArgs const& args,
                        std::filesystem::path const& scripts,
                        std::filesystem::path const& local_data) -> std::filesystem::path {
    if (!args.user_directory.empty()) {
        return std::filesystem::absolute(args.user_directory) / "Config/user-settings.scm";
    }

    if (!args.editor_scripts_directory.empty()) {
        return scripts / "Settings/user-settings.scm";
    }

    return local_data / "IsleOfJebbers/Config/user-settings.scm";
}

using PathsResult = std::expected<LaunchPaths, std::string>;

auto resolve_launch_paths(CliArgs const& args) -> PathsResult {
    auto const base{SDL_GetBasePath()};
    if (!base) {
        return PathsResult{std::unexpect, SDL_GetError()};
    }
    // SDL includes the trailing separator in its UTF-8 executable directory.
    auto directory{std::filesystem::path{reinterpret_cast<char8_t const*>(base)}.parent_path()};
    auto const project{directory.parent_path().parent_path()};
    std::filesystem::path mission;
    if (!args.mission_path.empty()) {
        mission = (project / args.mission_path).lexically_normal();
        if (!std::filesystem::is_regular_file(mission)) {
            return PathsResult{std::unexpect,
                               "Mission file does not exist: " + path_to_utf8(mission)};
        }
    }

    auto scripts{args.editor_scripts_directory.empty()
                     ? project / "LevelScripts"
                     : std::filesystem::absolute(args.editor_scripts_directory)};
    auto local_data{local_app_data()};
    if (!local_data) {
        return PathsResult{std::unexpect, std::move(local_data.error())};
    }

    auto settings_path{user_settings_path(args, scripts, *local_data)};

    auto user_root{project};
    if (!args.user_directory.empty()) {
        user_root = std::filesystem::absolute(args.user_directory);
    } else if (IOJ_SHIPPING ||
               std::filesystem::is_regular_file(project.parent_path() /
                                                "Engine/Build/InstalledProjectBuild.txt")) {
        user_root = *local_data / "Sandbox";
    }

    return PathsResult{std::in_place,
                       std::move(directory),
                       std::move(scripts),
                       std::move(settings_path),
                       std::move(mission),
                       user_root / "Saved/SaveGames"};
}

}

Application::Application(CliArgs args)
    : args_{std::move(args)}
    , capabilities_{query_game_capabilities()}
    , profile_browser_{
          [this] { return profiles::discover_profiles(profiles_); },
          [this](std::string const& id) { return profiles::load_report(profiles_, id); }} {}

auto Application::run() -> Result {
    auto paths{resolve_launch_paths(args_)};
    if (!paths) {
        return Result{std::unexpect, std::move(paths.error())};
    }

    paths_ = std::move(*paths);

    Window window;
    if (auto result{window.create()}; !result) {
        return result;
    }

    auto desktop{window.desktop_resolution()};
    if (!desktop) {
        return Result{std::unexpect, std::move(desktop.error())};
    }

    if (auto result{settings_.load(paths_.scripts_directory,
                                   paths_.user_settings_path,
                                   *desktop,
                                   args_.launch_display_mode)};
        !result) {
        return result;
    }

    if (auto display{initial_display(args_, settings_.saved_display())}) {
        auto actual{window.configure(*display)};
        if (!actual) {
            return Result{std::unexpect, std::move(actual.error())};
        }

        settings_.set_running_display(*actual);
    }

    profiles_ =
        profiles::ProfileManager{profiles::make_file_profile_storage(paths_.save_directory)};
    if (!profiles_.initialise()) {
        return Result{std::unexpect,
                      "Could not initialize profiles in " + path_to_utf8(paths_.save_directory)};
    }

    if (settings_.application().print_save_data) {
        std::println("Native save profile {}: {} profiles, {} outcomes",
                     profiles_.active_profile_id(),
                     profiles_.profiles().size(),
                     profiles_.active_records().size());
    }

    profile_browser_.refresh();
    IojPlayer_Attach(
        settings_, capabilities_, window.supported_resolutions(), profiles_, profile_browser_);
    if (auto result{start_engine(window, paths_, args_)}; !result) {
        return result;
    }

    Result play_result;
    try {
        play_result = play(window);
    } catch (std::exception const& error) {
        play_result = std::unexpected{std::string{error.what()}};
    } catch (...) {
        play_result = std::unexpected{"Unexpected exception while running the game"};
    }

    auto shutdown_result{shutdown_engine(window)};
    if (!play_result && !shutdown_result) {
        play_result.error() += "; " + shutdown_result.error();
    }
    if (!play_result) {
        return play_result;
    }
    return shutdown_result;
}
auto Application::play(Window& window) -> Result {
    auto const mode{args_.launch_display_mode};
    if (!IojPlayer_GameReady()) {
        return Result{std::unexpect,
                      "Unreal did not initialize a playable world. See the game log."};
    }

    if (!IojPlayer_ApplyPreferences()) {
        return Result{std::unexpect, "Could not apply player preferences. See the game log."};
    }

    if (mode != LaunchDisplayMode::Headless) {
        if (auto result{window.show(settings_)}; !result) {
            return result;
        }
    }

    while (!IojPlayer_ExitRequested()) {
        IojPlayer_BeginFrame();
        SDL_Event event;
        bool close_requested{};
        bool viewport_changed{};
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    close_requested = true;
                    break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                case SDL_EVENT_WINDOW_MOVED:
                    viewport_changed = true;
                    break;
                case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
                case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
                case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
                    if (auto result{window.refresh_resolutions()}; !result) {
                        return result;
                    }
                    break;
            }
        }
        if (close_requested || IojPlayer_ExitRequested()) {
            break;
        }

        if (viewport_changed) {
            if (auto result{window.synchronize_resize(settings_)}; !result) {
                return result;
            }
        }

        if (auto result{tick_engine()}; !result) {
            return result;
        }

    }

    return {};
}

}
