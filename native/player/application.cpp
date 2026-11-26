#include "player/application.h"

#include "player/cli_args.h"
#include "player/engine.h"
#include "player/window.h"
#include <ioj/files.h>
#include <ioj/host/game_host_api.h>
#include <ioj/host/level_host.h>
#include <ioj/host/mission_startup.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/profiles/file_profile_storage.h>
#include <ioj/profiles/level_score.h>
#include <ioj/profiles/sample_profile_records.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <ioj/system_capabilities.h>

#include <sbx/memory_resource.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>

#include <chrono>
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
#include <variant>

namespace ioj {
namespace {
auto user_settings_path(std::filesystem::path const& user_directory,
                        std::filesystem::path const& editor_scripts_directory,
                        std::filesystem::path const& local_data) -> std::filesystem::path {
    if (!user_directory.empty()) {
        return user_directory / "Config/user-settings.scm";
    }

    if (!editor_scripts_directory.empty()) {
        return editor_scripts_directory / "Settings/user-settings.scm";
    }

    return local_data / "IsleOfJebbers/Config/user-settings.scm";
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
    profiles::ProfileManager manager{profiles::make_file_profile_storage(save_directory)};
    if (!manager.initialise()) {
        return std::unexpected{"Could not initialize profiles in " + path_to_utf8(save_directory)};
    }

    return manager;
}
}

Application::Application(profiles::ProfileManager profiles)
    : profiles_{std::move(profiles)}
    , level_arena_{memory::BumpResource::default_capacity_bytes,
                   *sbx::memory::mimalloc_resource()} {}

auto Application::run(CliArgs args) -> ApplicationResult {
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

    if (!args.mission_path.empty()) {
        args.mission_path = (project_directory / args.mission_path).lexically_normal();
        if (!std::filesystem::is_regular_file(args.mission_path)) {
            return Result{std::unexpect,
                          "Mission file does not exist: " + path_to_utf8(args.mission_path)};
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

    Application application{std::move(*profile_manager)};

    Window window;
    if (auto result{window.create()}; !result) {
        return result;
    }

    auto desktop{window.desktop_resolution()};
    if (!desktop) {
        return Result{std::unexpect, std::move(desktop.error())};
    }

    auto& settings{application.settings_};
    if (auto result{
            settings.load(scripts_directory, settings_path, *desktop, args.launch_display_mode)};
        !result) {
        return result;
    }

    if (auto display{initial_display(
            settings.launch_display_mode(), args.initial_window_size, settings.saved_display())}) {
        auto actual{window.configure(*display)};
        if (!actual) {
            return Result{std::unexpect, std::move(actual.error())};
        }

        settings.set_running_display(*actual);
    }

    if (settings.application().print_save_data) {
        std::println("Native save profile {}: {} profiles, {} outcomes",
                     application.profiles_.active_profile_id(),
                     application.profiles_.profiles().size(),
                     application.profiles_.active_records().size());
    }

    application.profile_browser_.refresh(application.profiles_);
    auto const capabilities{query_system_capabilities()};
    auto const engine_command_line{
        engine_arguments(project_directory, settings.launch_display_mode(), args.user_directory)};
    IojPlayer_Attach(settings,
                     capabilities,
                     window.supported_resolutions(),
                     application.profiles_,
                     application.profile_browser_);
    if (auto result{start_engine(window, engine_command_line, settings.launch_display_mode())};
        !result) {
        return result;
    }

    Result play_result;
    try {
        play_result = application.play(window, args.mission_path, args.exit_when_ready);
    } catch (std::exception const& error) {
        play_result = std::unexpected{std::string{error.what()}};
    } catch (...) {
        play_result = std::unexpected{"Unexpected exception while running the game"};
    }

    try {
        application.unload_level(sim::RunEndReason::WorldEnd, false);
    } catch (...) {
        shutdown_engine(window);
        throw;
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
auto Application::play(Window& window,
                       std::filesystem::path const& mission_path,
                       bool exit_when_ready) -> ApplicationResult {
    if (!IojPlayer_GameReady()) {
        return ApplicationResult{std::unexpect,
                      "Unreal did not initialize a playable world. See the game log."};
    }

    if (!IojPlayer_ApplyPreferences()) {
        return ApplicationResult{std::unexpect, "Could not apply player preferences. See the game log."};
    }

    if (settings_.launch_display_mode() != LaunchDisplayMode::Headless) {
        if (auto result{window.show(settings_)}; !result) {
            return result;
        }
    }

    if (!mission_path.empty()) {
        if (!launch_mission(mission_path)) {
            return ApplicationResult{std::unexpect,
                                     "Mission launch failed: " + path_to_utf8(mission_path)};
        }

        host::open_mission();
    }

    auto const deadline{std::chrono::steady_clock::now() + std::chrono::minutes{2}};
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

        if (exit_when_ready) {
            auto const startup{IojPlayer_MissionStartup()};
            if (mission_path.empty() || startup == MissionStartup::Ready) {
                std::println("Ready: {}",
                             mission_path.empty() ? "MainMenu" : path_to_utf8(mission_path));
                return {};
            }

            if (startup == MissionStartup::Failed || std::chrono::steady_clock::now() >= deadline) {
                return ApplicationResult{std::unexpect,
                              "Mission did not reach simulation readiness. See the game log."};
            }
        }

        if (auto result{tick_engine()}; !result) {
            return result;
        }

        auto const messages{host::take_messages()};
        process_preferences(window, messages.preference_edits);
        process_profile_actions(messages.profile_actions);

        auto const& navigation{messages.navigation_request};
        if (auto const* menu{std::get_if<session::LevelSelectRequest>(&navigation)}) {
            pending_level_.reset();
            unload_level(sim::RunEndReason::WorldEnd, false);
            host::open_menu(menu->preferred_level_id.value, menu->launch_error);
            continue;
        }

        if (auto const* mission_request{std::get_if<session::PendingLevel>(&navigation)}) {
            unload_level(sim::RunEndReason::WorldEnd, false);
            pending_level_.emplace(*mission_request);
            host::open_mission();
            continue;
        }

        if (!level_) {
            if (pending_level_ && host::level_world_ready()) {
                create_level(*pending_level_);
                pending_level_.reset();
            }
            continue;
        }

        if (host::presents_level(*level_)) {
            level_->append_player_input(messages.ship_input);
            if (messages.requested_simulation_mode == levels::LaunchMode::Running &&
                level_->state() == sim::LevelState::Paused) {
                level_->start();
            } else if (messages.requested_simulation_mode == levels::LaunchMode::Paused &&
                       level_->state() == sim::LevelState::Running) {
                level_->pause();
            }

            if (auto delta_seconds{host::take_frame_delta_seconds()}) {
                advance_level(*delta_seconds);
            }
        }
    }

    if (exit_when_ready) {
        return ApplicationResult{std::unexpect, "Engine exited before readiness"};
    }

    return {};
}

void Application::process_profile_actions(std::span<host::ProfileAction const> actions) {
    using enum host::ProfileActionKind;
    auto const count{actions.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& action{actions[index]};
        auto result{profiles::CreateProfileResult::succeeded};
        switch (action.kind) {
            case Create:
                result = profiles_.create_profile(action.profile_name_or_id).result;
                break;
            case Activate:
                if (!profiles_.activate_profile(action.profile_name_or_id)) {
                    result = profiles::CreateProfileResult::persistence_failed;
                }
                break;
            case ResetTestProfile:
#if IOJ_SHIPPING
                std::unreachable();
#else
                if (!profiles_.reset_test_profile(profiles::sample_profile_records())) {
                    result = profiles::CreateProfileResult::persistence_failed;
                }
#endif
                break;
            case Refresh:
                break;
            case SelectReport:
                if (!profile_browser_.load_profile_report(profiles_, action.profile_name_or_id)) {
                    result = profiles::CreateProfileResult::persistence_failed;
                }
                break;
        }

        if (result == profiles::CreateProfileResult::succeeded && action.kind != SelectReport) {
            profile_browser_.refresh(profiles_);
        }

        if (result == profiles::CreateProfileResult::persistence_failed) {
            std::println(stderr, "Profile operation failed for '{}'", action.profile_name_or_id);
        }

        host::profile_result(index, result);
    }
}

void Application::process_preferences(Window& window, std::span<host::PreferenceEdit const> edits) {
    auto const count{edits.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& edit{edits[index]};
        if (edit.action != host::PreferenceAction::Save) {
            auto result{edit.action == host::PreferenceAction::PreviewDisplay
                            ? window.preview({edit.values.window_mode, edit.values.resolution})
                            : window.revert_preview()};
            host::preference_result(index, result ? std::string_view{} : result.error());
            continue;
        }

        auto result{edit.category == GameSettingCategory::Video ? window.verify_preview()
                                                                : ApplicationResult{}};
        if (result) {
            result = settings_.save_category(edit.category, edit.values, edit.original_settings);
        }
        if (!result) {
            if (edit.category == GameSettingCategory::Video) {
                if (auto rollback{window.revert_preview()}; !rollback) {
                    result.error() += "; rollback failed: " + rollback.error();
                }
            }
            host::preference_result(index, result.error());
            continue;
        }

        switch (edit.category) {
            case GameSettingCategory::Video:
                settings_.apply(video_settings(edit.values));
                window.keep_preview(settings_);
                break;
            case GameSettingCategory::Audio:
                settings_.apply(audio_settings(edit.values));
                break;
            case GameSettingCategory::Controls:
                settings_.apply(controls_settings(edit.values));
                break;
            case GameSettingCategory::Gameplay:
            case GameSettingCategory::Accessibility:
                break;
        }

        host::preference_result(index, {});
    }
}

auto Application::launch_mission(std::filesystem::path const& path) -> bool {
    auto definition{levels::authoring::DefinitionReader::for_level(path).read_level_file(path)};
    if (!definition) {
        auto error{levels::format_diagnostics(definition.error())};
        std::println(stderr, "Mission launch failed: {}", error);
        return false;
    }

    levels::LaunchOptions options;
    options.control_context =
        definition->camera() ? levels::ControlContext::Observer : levels::ControlContext::Player;
    pending_level_.emplace(std::move(*definition), path_to_utf8(path), options);
    return true;
}
void Application::create_level(session::PendingLevel& pending) {
    pending.options.launch_mode = settings_.application().start_levels_paused
                                    ? levels::LaunchMode::Paused
                                    : levels::LaunchMode::Running;

    sim::LevelSimInitData inputs;
    if (!host::prepare_level(pending, inputs.player)) {
        return;
    }

    auto const& options{pending.options};
    auto const mode{options.launch_mode};
    sim::LevelRunTiming const timing{
        .time_scale = PositiveFinite<double>::make(options.requested_time_scale.get()),
        .duration = options.simulated_duration_seconds.transform(
            [](auto value) { return Duration::make(value); }),
    };
    auto built{sim::make_level_simulation_init_data(pending.definition, std::move(inputs), timing)};
    if (!built) {
        std::string errors;
        for (auto const& message : built.error()) {
            errors += message + "\n";
        }

        host::level_start_failed(errors);
        return;
    }

    built->collision.static_bounds.reset();
    for (auto const& bound : host::static_geometry(built->collision.grid_spec)) {
        built->collision.static_bounds.add(bound.min, bound.max);
    }

    level_.emplace(std::move(*built), level_arena_);
    if (!host::bind_level(pending, *level_)) {
        unload_level(sim::RunEndReason::WorldEnd, false);
        return;
    }

    if (level_->result()) {
        unload_level(sim::RunEndReason::LevelCompleted, true);
    } else if (mode == levels::LaunchMode::Running && level_->state() == sim::LevelState::Paused) {
        level_->start();
    }
}
void Application::advance_level(double delta_seconds) {
    auto const state{level_->state()};
    auto const previous_tick{level_->clock().get_completed_ticks()};
    level_->advance(delta_seconds);
    if (level_->state() == sim::LevelState::Running ||
        level_->clock().get_completed_ticks() != previous_tick) {
        host::present_level(delta_seconds);
    }

    if (level_->result()) {
        unload_level(sim::RunEndReason::LevelCompleted, true);
    } else if (state == sim::LevelState::Running && level_->state() == sim::LevelState::Paused) {
        host::simulation_paused();
    }
}
void Application::unload_level(sim::RunEndReason reason, bool show_results) {
    if (!level_) {
        return;
    }

    level_->stop_run(reason);
    auto const completed{profiles::completed_level(*level_).value()};
    auto const score{profiles::make_level_score(completed, profiles::current_timestamp())};
    profiles::RecordSaveResult saved{};
    if (score) {
        saved = profiles_.append_score_record(*score);
        if (saved.persisted) {
            profile_browser_.refresh(profiles_);
        } else {
            std::println(stderr, "Failed to save the score for {}", completed.metadata.id.value);
        }
    }

    if (host::presents_level(*level_)) {
        host::finish_level(completed, score.has_value(), saved.new_best_time, show_results);
    }

    level_.reset();
    level_arena_.reset();
}
}
