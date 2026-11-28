#include "player/application.h"
#include "player/cli_args.h"
#include <ioj/files.h>
#include <ioj/profiles/file_profile_storage.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <exception>
#include <print>
#include <utility>

namespace ioj {
namespace {
auto user_settings_path(std::filesystem::path const& user_directory,
                        std::filesystem::path const& editor_scripts_directory,
                        std::filesystem::path const& local_data) -> std::filesystem::path {
    if (!user_directory.empty()) {
        return user_directory / "Config/preferences.scm";
    }

    if (!editor_scripts_directory.empty()) {
        return editor_scripts_directory / "Settings/preferences.scm";
    }

    return local_data / "IsleOfJebbers/Config/preferences.scm";
}

auto open_profiles(std::filesystem::path const& project_directory,
                   std::filesystem::path const& user_directory,
                   std::filesystem::path const& local_data)
    -> std::expected<profiles::ProfileManager, std::string> {
    auto user_root{project_directory};
    if (!user_directory.empty()) {
        user_root = user_directory;
    } else if (IOJ_SHIPPING ||
               std::filesystem::is_regular_file(project_directory.parent_path() /
                                                "Engine/Build/InstalledProjectBuild.txt")) {
        user_root = local_data / "Sandbox";
    }

    auto const save_directory{user_root / "Saved/SaveGames"};
    auto manager{
        profiles::ProfileManager::open(profiles::make_file_profile_storage(save_directory))};
    if (!manager) {
        return std::unexpected{manager.error() + " in " + path_to_utf8(save_directory)};
    }

    return manager;
}

auto run_player(CliArgs args) -> ApplicationResult {
    using Result = ApplicationResult;

    auto const base{SDL_GetBasePath()};
    if (!base) {
        return Result{std::unexpect, SDL_GetError()};
    }

    // SDL includes the trailing separator in its UTF-8 executable directory.
    auto const binary_directory{
        std::filesystem::path{reinterpret_cast<char8_t const*>(base)}.parent_path()};
    auto const project_directory{binary_directory.parent_path().parent_path()};

    if (!args.user_directory.empty()) {
        args.user_directory = std::filesystem::absolute(args.user_directory);
    }

    if (!args.editor_scripts_directory.empty()) {
        args.editor_scripts_directory = std::filesystem::absolute(args.editor_scripts_directory);
    }

    if (!args.level_path.empty()) {
        args.level_path = (project_directory / args.level_path).lexically_normal();
        if (!std::filesystem::is_regular_file(args.level_path)) {
            return Result{std::unexpect,
                          "Level file does not exist: " + path_to_utf8(args.level_path)};
        }
    }

    auto const scripts_directory{args.editor_scripts_directory.empty()
                                     ? project_directory / "LevelScripts"
                                     : args.editor_scripts_directory};
    auto local_data{local_app_data()};
    if (!local_data) {
        return Result{std::unexpect, std::move(local_data.error())};
    }

    auto const settings_path{
        user_settings_path(args.user_directory, args.editor_scripts_directory, *local_data)};

    auto profile_manager{open_profiles(project_directory, args.user_directory, *local_data)};
    if (!profile_manager) {
        return Result{std::unexpect, std::move(profile_manager.error())};
    }

    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    struct SdlLifetime {
        ~SdlLifetime() { SDL_Quit(); }
    } const sdl;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        return std::unexpected{std::string{SDL_GetError()}};
    }
    auto window{Window::create()};
    if (!window) {
        return std::unexpected{std::move(window.error())};
    }

    auto desktop{window->desktop_resolution()};
    if (!desktop) {
        return Result{std::unexpect, std::move(desktop.error())};
    }

    PlayerSettings settings;
    if (auto result{
            settings.load(scripts_directory, settings_path, *desktop, args.launch_display_mode)};
        !result) {
        return result;
    }

    if (auto display{initial_display(
            settings.launch_display_mode(), args.initial_window_size, settings.saved_display())}) {
        auto actual{window->configure(*display)};
        if (!actual) {
            return Result{std::unexpect, std::move(actual.error())};
        }

        settings.set_running_display(*actual);
    }

    if (settings.application().print_save_data) {
        std::println("Native save profile {}: {} profiles, {} outcomes",
                     profile_manager->active_profile_id(),
                     profile_manager->profiles().size(),
                     profile_manager->active_records().size());
    }

    auto const arguments{
        engine_arguments(project_directory, settings.launch_display_mode(), args.user_directory)};
    Application application{std::move(*window),
                            std::move(settings),
                            std::move(*profile_manager),
                            scripts_directory,
                            args.exit_when_ready};
    return application.run(arguments, args.level_path);
}
}
}
auto main(int argc, char** argv) -> int {
    using namespace ioj;
    try {
        auto cli_args{parse_cli_args(argc, argv)};
        if (!cli_args) {
            return cli_args.error();
        }

        if (auto result{run_player(std::move(*cli_args))}; !result) {
            std::println(stderr, "{}", result.error());
            return 1;
        }

        return 0;
    } catch (std::exception const& error) {
        std::println(stderr, "{}", error.what());
        return 1;
    }
}
