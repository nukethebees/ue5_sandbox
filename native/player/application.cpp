#include "player/application.h"

#include "player/cli_args.h"
#include "player/engine.h"
#include "player/input/input_system.h"
#include "player/loaded_level.h"
#include "player/native_endpoint.h"
#include "player/window.h"
#include <ioj/bridge/completed_level.h>
#include <ioj/bridge/level_catalog.h>
#include <ioj/bridge/level_frame.h>
#include <ioj/files.h>
#include <ioj/host/game_host_api.h>
#include <ioj/input/dispatch/player_menu.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/presentation/level_catalog.h>
#include <ioj/presentation/profile_view.h>
#include <ioj/profiles/file_profile_storage.h>
#include <ioj/profiles/level_score.h>
#include <ioj/profiles/sample_profile_records.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <ioj/system_capabilities.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <expected>
#include <filesystem>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

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

auto initialize_presentation(LoadedLevel& level, host::NativeEndpoint& endpoint) -> std::string {
    auto const mode{level.presentation_setup->launch_mode};
    auto& simulation{level.simulation};
    endpoint.initialize_level({bridge::level_init(*level.presentation_setup, simulation),
                               bridge::level_frame(simulation)});
    auto const& reply{endpoint.exchange()};
    assert(!reply.level_results.empty());
    auto error{reply.level_results.back()};
    level.presentation_setup.reset();

    if (error.empty() && mode == levels::LaunchMode::Running &&
        simulation.state() == sim::LevelState::Paused) {
        simulation.start();
    }
    return error;
}
void
    advance_level(sim::LevelSim& simulation, double delta_seconds, host::NativeEndpoint& endpoint) {
    auto const state{simulation.state()};
    simulation.advance(delta_seconds);
    auto const frame{bridge::level_frame(simulation)};
    auto const visual_delta{simulation.state() == sim::LevelState::Running ||
                                    !frame.events.ticks.empty()
                                ? delta_seconds
                                : 0.0};
    endpoint.present_frame({visual_delta, frame});

    if (state == sim::LevelState::Running && simulation.state() == sim::LevelState::Paused) {
        endpoint.send(host::NativeMessageKind::SimulationPaused);
    }
    endpoint.exchange();
}
}

Application::Application(profiles::ProfileManager profiles)
    : profiles_{std::move(profiles)} {}

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
    LevelStorage storage;
    application.menu_.show_reset_profile = !IOJ_SHIPPING;
    application.menu_.draft.begin(settings.view().applied_values, settings.view().defaults);
    IojPlayer_Attach(capabilities, application.menu_);
    host::present_settings(settings.view(), window.supported_resolutions());
    application.publish_profiles();
    if (auto result{start_engine(window, engine_command_line, settings.launch_display_mode())};
        !result) {
        return result;
    }

    Result play_result;
    try {
        play_result = application.play(
            window, storage, args.level_path, scripts_directory, args.exit_when_ready);
    } catch (std::exception const& error) {
        play_result = std::unexpected{std::string{error.what()}};
    } catch (...) {
        play_result = std::unexpected{"Unexpected exception while running the game"};
    }

    try {
        application.unload_level(storage, sim::RunEndReason::WorldEnd, false);
    } catch (...) {
        if (auto shutdown{shutdown_engine(window)}; !shutdown) {
            std::println(
                stderr, "Engine shutdown failed during level cleanup: {}", shutdown.error());
        }

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
struct Application::PlayerInputSession {
    PlayerInputSession(Application& owner,
                       Window& game_window,
                       PlayerSettings const& player_settings,
                       PlayerMenu& player_menu,
                       LevelStorage& levels)
        : application{owner}
        , endpoint{owner.endpoint_}
        , storage{levels}
        , window{game_window}
        , settings{player_settings}
        , menu{player_menu}
        , input{*window.sdl_window(), settings.applied().controls, endpoint} {
        endpoint.present_keyboard(&input.keyboard());
    }

    auto routing() const -> InputRoute {
        if (menu.screen == MenuScreen::Pause) {
            return {InputContext::Menu, !menu.pause_blocked()};
        }
        auto const* level{storage.get()};
        return {level ? level->input_context() : InputContext::Menu};
    }
    void dispatch(host::PlayerAction action) {
        auto* level{storage.get()};
        if (!level || level->presentation_setup) {
            return;
        }
        using enum host::PlayerAction;
        switch (action) {
            case OpenPause:
                if (menu.screen != MenuScreen::Closed ||
                    level->simulation.state() == sim::LevelState::Stopped) {
                    return;
                }
                menu.open(MenuScreen::Pause);
                level->update_pause(true);
                endpoint.present_pause({host::PauseCommandKind::Open, host::PauseMenuItem::Resume});
                break;
            case RequestResume:
                if (menu.screen == MenuScreen::Pause && !menu.pause_blocked()) {
                    choose_pause(host::PauseMenuItem::Resume);
                }
                break;
            case BenchmarkNext:
                level->end_selected = !level->end_selected;
                endpoint.select_benchmark(level->end_selected);
                break;
            case BenchmarkActivate:
                dispatch(level->end_selected ? EndBenchmark : ToggleBenchmark);
                break;
            case ToggleBenchmark:
                level->set_simulation_mode(level->simulation.state() == sim::LevelState::Running
                                               ? levels::LaunchMode::Paused
                                               : levels::LaunchMode::Running);
                break;
            case EndBenchmark:
                return_requested = true;
                break;
        }
    }
    void choose_pause(host::PauseMenuItem item) {
        if (item != menu.pause_item() && !menu.leave_ready()) {
            menu.pending_pause = item;
            present_menu({menu.focus, MenuOperation::Focus});
            return;
        }
        using enum host::PauseMenuItem;
        switch (item) {
            case Resume:
                menu.open(MenuScreen::Closed);
                storage.get()->update_pause(false);
                endpoint.present_pause({host::PauseCommandKind::Finish, item});
                break;
            case Controls:
            case Audio:
                menu.select_page(item == Audio ? MainMenuItem::Audio : MainMenuItem::Controls);
                menu.pause_sidebar = false;
                menu.select(menu.primary_target(settings.view()));
                endpoint.present_pause({host::PauseCommandKind::ShowPage, item});
                break;
            case LevelSelect:
                return_requested = true;
                break;
            case Quit:
                quit_requested = true;
                break;
        }
    }
    void finish_leave(bool accepted) {
        if (auto item{std::exchange(menu.pending_pause, {})}; item && accepted) {
            choose_pause(*item);
        } else if (auto update{menu.finish_main_leave(accepted)}) {
            present_menu(*update);
        }
    }

    void present_menu(MenuUpdate const& update) {
        if (update.operation == MenuOperation::Activate) {
            if (update.target.kind == MenuTarget::Kind::Profile ||
                update.target.kind == MenuTarget::Kind::Outcome) {
                application.publish_profiles();
            } else if (update.target.kind == MenuTarget::Kind::Control) {
                switch (update.target.control()) {
                    case MenuControl::ConfirmCreate:
                    case MenuControl::ActivateProfile:
                    case MenuControl::RefreshProfiles:
                    case MenuControl::ResetProfile:
                        application.profile_action(update.target.control());
                        return;
                    default:
                        break;
                }
            }
        }
        if (menu.prompt == OptionsPrompt::Dirty && update.operation == MenuOperation::Activate) {
            auto const choice{update.target.control()};
            menu.set_prompt(OptionsPrompt::None);
            if (choice == MenuControl::Discard) {
                menu.draft.cancel();
            }
            host::present_menu({menu.focus, MenuOperation::Focus});
            if (choice == MenuControl::Save) {
                host::present_menu(update);
            } else {
                finish_leave(choice == MenuControl::Discard);
            }
        } else {
            host::present_menu(update);
        }
    }
    void process_widget_actions() {
        auto const& replies{endpoint.exchange()};
        for (std::size_t cursor{}; cursor < replies.order.size(); ++cursor) {
            auto const [kind, index]{replies.order[cursor]};
            switch (kind) {
                case host::UnrealMessageKind::PauseInteraction: {
                    auto const event{replies.pause_events[index]};
                    if (menu.screen != MenuScreen::Pause) {
                        break;
                    }
                    switch (event.kind) {
                        case host::PauseWidgetEventKind::Activate:
                            if (!menu.pause_blocked()) {
                                choose_pause(event.item);
                            }
                            break;
                        case host::PauseWidgetEventKind::Select:
                            if (menu.select_pause_item(event.item)) {
                                endpoint.present_pause(
                                    {host::PauseCommandKind::Select, event.item});
                            }
                            break;
                        case host::PauseWidgetEventKind::ContentFocused:
                            if (!menu.pause_blocked()) {
                                menu.pause_sidebar = false;
                            }
                            break;
                    }
                    break;
                }
                case host::UnrealMessageKind::Status:
                    application.engine_ = replies.status[index];
                    if (application.engine_.failed) {
                        pause_presentation = std::unexpected{"Unreal reported a critical error"};
                    }
                    break;
                case host::UnrealMessageKind::SimulationMode:
                    if (auto* level{storage.get()};
                        level && !level->presentation_setup && menu.screen != MenuScreen::Pause) {
                        level->set_simulation_mode(replies.simulation_modes[index]);
                    }
                    break;
                case host::UnrealMessageKind::LevelInitialized:
                    break;
                case host::UnrealMessageKind::PausePresented:
                    if (!replies.pause_results[index]) {
                        pause_presentation = std::unexpected{
                            "Could not present the native pause menu. See the game log."};
                    }
                    break;
                case host::UnrealMessageKind::RefreshProfiles:
                    application.profile_action(MenuControl::RefreshProfiles);
                    break;
                case host::UnrealMessageKind::ResetTestProfile:
                    application.profile_action(MenuControl::ResetProfile);
                    break;
                case host::UnrealMessageKind::Setting: {
                    auto const& change{replies.settings[index]};
                    if (!menu.saving && menu.prompt == OptionsPrompt::None &&
                        menu.draft.binding_edit().prompt == BindingPrompt::Closed &&
                        settings.view().set_setting(menu.draft,
                                                    change.setting,
                                                    change.value,
                                                    window.supported_resolutions())) {
                        menu.select(change.setting);
                        present_menu({menu.focus, MenuOperation::RefreshSettings});
                    }
                    break;
                }
                case host::UnrealMessageKind::EditText: {
                    auto const& edit{replies.edits[index]};
                    if (auto update{menu.edit_text(edit.control, edit.value)}) {
                        present_menu(*update);
                    }
                    break;
                }
                case host::UnrealMessageKind::Action:
                    dispatch(replies.actions[index]);
                    break;
                case host::UnrealMessageKind::Hit:
                    if (auto update{menu.activate(replies.hits[index],
                                                  settings.view(),
                                                  window.supported_resolutions())}) {
                        present_menu(*update);
                    }
                    break;
                default:
                    std::unreachable();
            }
        }
        endpoint.release_replies();
        for (;;) {
            auto const actions{host::take_widget_actions()};
            if (actions.empty()) {
                break;
            }
            for (auto const& action : actions) {
                if (auto const* result{std::get_if<host::SettingsSaveResult>(&action)}) {
                    finish_leave(result->succeeded);
                } else {
                    dispatch(std::get<host::PlayerAction>(action));
                }
            }
        }
    }
    void navigate_menu(MenuButton button) {
        if (auto update{menu.navigate(
                button.command, button.repeat, settings.view(), window.supported_resolutions())}) {
            present_menu(*update);
        }
    }
    void navigate(MenuButton button) {
        if (!button.down) {
            return;
        }
        if (menu.draft.binding_edit().prompt != BindingPrompt::Closed) {
            menu.draft.navigate_binding(button.command, button.repeat);
            host::present_menu({menu.draft.binding_edit().mapping, MenuOperation::BindingPrompt});
        } else if (menu.screen == MenuScreen::Pause &&
                   (menu.pause_sidebar || button.command == MenuCommand::Back) &&
                   !menu.pause_blocked()) {
            auto const before{menu.pause_selection};
            if (auto item{menu.navigate_pause(button.command, button.repeat)}) {
                choose_pause(*item);
            } else if (before != menu.pause_selection) {
                endpoint.present_pause({host::PauseCommandKind::Select, menu.pause_selection});
            }

        } else {
            navigate_menu(button);
        }
    }
    void consume_commands() {
        for (auto const& command : input.commands()) {
            switch (command.kind) {
                case InputCommand::Kind::Action:
                    dispatch(command.action);
                    break;
                case InputCommand::Kind::Capture:
                    menu.draft.capture_binding(command.key);
                    host::present_menu(
                        {menu.draft.binding_edit().mapping, MenuOperation::BindingPrompt});
                    break;
                case InputCommand::Kind::Navigation:
                    navigate(command.button);
                    break;
            }
        }
        input.clear_commands();
    }
    auto advance(double seconds) -> WindowInputEvents {
        auto const events{input.poll()};
        if (events.keyboard_changed) {
            endpoint.present_keyboard(&input.keyboard());
        }
        auto const& batch{input.events()};
        std::size_t cursor{};
        for (;;) {
            input.synchronize(routing(), menu.input_mode());
            if (!input.commands().empty()) {
                consume_commands();
                process_widget_actions();
                continue;
            }
            if (cursor == batch.order.size()) {
                break;
            }
            cursor = input.map_events(batch, cursor);
            consume_commands();
            process_widget_actions();
        }
        input.repeat_navigation(seconds);
        consume_commands();
        process_widget_actions();
        return events;
    }

    Application& application;
    host::NativeEndpoint& endpoint;
    LevelStorage& storage;
    Window& window;
    PlayerSettings const& settings;
    PlayerMenu& menu;
    InputSystem input;
    ApplicationResult pause_presentation{};
    bool return_requested{};
    bool quit_requested{};
};

auto Application::play(Window& window,
                       LevelStorage& storage,
                       std::filesystem::path const& level_path,
                       std::filesystem::path const& scripts_directory,
                       bool exit_when_ready) -> ApplicationResult {
    PlayerInputSession play_input{*this, window, settings_, menu_, storage};
    play_input.process_widget_actions();
    if (!engine_.ready) {
        return ApplicationResult{std::unexpect,
                                 "Unreal did not initialize a playable world. See the game log."};
    }

    if (!IojPlayer_ApplyPreferences()) {
        return ApplicationResult{std::unexpect,
                                 "Could not apply player preferences. See the game log."};
    }

    if (settings_.launch_display_mode() != LaunchDisplayMode::Headless) {
        if (auto result{window.show(settings_)}; !result) {
            return result;
        }
        host::present_settings(settings_.view(), window.supported_resolutions());
    }

    if (!level_path.empty()) {
        if (auto result{launch_level(storage, level_path)}; !result) {
            return result;
        }
    }

    constexpr double max_frame_delta_seconds{0.1};
    auto last_input_frame{std::chrono::steady_clock::now()};
    auto const deadline{last_input_frame + std::chrono::minutes{2}};
    std::vector<AvailableLevel> available_levels;

    while (!engine_.exit_requested) {
        IojPlayer_BeginFrame();
        if (menu_.prompt == OptionsPrompt::Display && !menu_.saving &&
            menu_.display_seconds_remaining() == 0) {
            host::present_menu({MenuControl::RevertDisplay, MenuOperation::Activate});
        }
        play_input.process_widget_actions();
        auto const input_time{std::chrono::steady_clock::now()};
        auto const input_delta{std::chrono::duration<double>{input_time - last_input_frame}};
        last_input_frame = input_time;
        auto const frame_seconds{std::min(input_delta.count(), max_frame_delta_seconds)};
        auto const window_events{play_input.advance(frame_seconds)};
        if (auto step{play_input.input.viewer_step(frame_seconds)}) {
            endpoint_.step_viewer(*step);
        }
        if (play_input.routing().context == InputContext::Gameplay) {
            endpoint_.present_steering(play_input.input.steering());
        }
        if (endpoint_.size() != 0) {
            play_input.process_widget_actions();
        }
        if (window_events.display_changed) {
            if (auto result{window.refresh_resolutions()}; !result) {
                return result;
            }
            host::present_settings(settings_.view(), window.supported_resolutions());
        }
        if (!play_input.pause_presentation) {
            return play_input.pause_presentation;
        }
        if (window_events.close_requested || play_input.quit_requested || engine_.exit_requested) {
            break;
        }

        if (window_events.viewport_changed) {
            if (auto result{window.synchronize_resize(settings_)}; !result) {
                return result;
            }
            host::present_settings(settings_.view(), window.supported_resolutions());
        }

        if (exit_when_ready) {
            if (level_path.empty()) {
                std::println("Ready: MainMenu");
                return {};
            }

            if (std::chrono::steady_clock::now() >= deadline) {
                return ApplicationResult{
                    std::unexpect, "Timed out waiting for level readiness. See the game log."};
            }
        }

        if (auto result{tick_engine()}; !result) {
            return result;
        }

        play_input.process_widget_actions();
        auto const messages{host::take_messages()};
        if (play_input.return_requested) {
            auto const id{storage.get()->simulation.level_metadata().id};
            unload_level(storage, sim::RunEndReason::WorldEnd, false);
            menu_.selected_level = id.value;
            menu_.open(MenuScreen::Main, !menu_.selected_level.empty());
            endpoint_.open_menu({});
            endpoint_.exchange();
            play_input.return_requested = false;
            continue;
        }
        if (process_preferences(window, messages.preference_edits)) {
            play_input.input.refresh_bindings();
            play_input.consume_commands();
        }

        if (messages.refresh_catalog) {
            available_levels = publish_catalog(scripts_directory);
        }

        auto const& navigation{messages.navigation_request};
        if (exit_when_ready && !std::holds_alternative<std::monostate>(navigation)) {
            return ApplicationResult{std::unexpect, "Level startup was cancelled"};
        }
        if (auto const* menu{std::get_if<session::LevelSelectRequest>(&navigation)}) {
            unload_level(storage, sim::RunEndReason::WorldEnd, false);
            menu_.selected_level = menu->preferred_level_id.value;
            menu_.open(MenuScreen::Main, !menu_.selected_level.empty());
            endpoint_.open_menu(menu->launch_error);
            endpoint_.exchange();
            continue;
        }

        if (auto const* selection{std::get_if<host::LevelSelection>(&navigation)}) {
            unload_level(storage, sim::RunEndReason::WorldEnd, false);
            auto const entry{std::ranges::find_if(available_levels, [&](auto const& candidate) {
                return candidate.definition.metadata().id == selection->id;
            })};
            if (entry != available_levels.end()) {
                auto result{launch_level(storage,
                                         entry->definition,
                                         path_to_utf8(entry->source_path),
                                         selection->options)};
                available_levels = std::vector<AvailableLevel>{};
                if (!result) {
                    menu_.selected_level = selection->id.value;
                    menu_.open(MenuScreen::Main, !menu_.selected_level.empty());
                    endpoint_.open_menu(result.error());
                    endpoint_.exchange();
                }
                continue;
            }
            menu_.selected_level = selection->id.value;
            menu_.open(MenuScreen::Main, !menu_.selected_level.empty());
            endpoint_.open_menu("The selected level is no longer available.");
            endpoint_.exchange();
            continue;
        }

        auto* const level{storage.get()};
        if (!level) {
            continue;
        }

        if (level->presentation_setup && engine_.world_ready) {
            auto const start_paused{level->presentation_setup->launch_mode ==
                                        levels::LaunchMode::Paused &&
                                    (level->control_context == levels::ControlContext::Player ||
                                     level->control_context == levels::ControlContext::Observer)};
            if (auto error{initialize_presentation(*level, endpoint_)}; !error.empty()) {
                menu_.selected_level = level->simulation.level_metadata().id.value;
                unload_level(storage, sim::RunEndReason::WorldEnd, false);
                if (exit_when_ready) {
                    return ApplicationResult{std::unexpect, std::move(error)};
                }
                menu_.open(MenuScreen::Main, !menu_.selected_level.empty());
                endpoint_.open_menu(error);
                endpoint_.exchange();
                continue;
            }

            if (start_paused) {
                play_input.dispatch(host::PlayerAction::OpenPause);
            }
            if (level->simulation.state() == sim::LevelState::Stopped) {
                unload_level(storage, sim::RunEndReason::LevelCompleted, true);
            }
            if (exit_when_ready) {
                std::println("Ready: {}", path_to_utf8(level_path));
                return {};
            }

            level->previous_frame = std::chrono::steady_clock::now();
            continue;
        }

        if (engine_.presents_level) {
            level->simulation.append_player_input(play_input.input.ship_input());

            advance_level(level->simulation, level->elapsed_seconds(), endpoint_);
            level->update_pause(menu_.screen == MenuScreen::Pause);
            if (level->simulation.state() == sim::LevelState::Stopped) {
                unload_level(storage, sim::RunEndReason::LevelCompleted, true);
            }
        }
    }

    if (exit_when_ready) {
        return ApplicationResult{std::unexpect, "Engine exited before readiness"};
    }

    return {};
}

auto Application::publish_catalog(std::filesystem::path const& scripts_directory)
    -> std::vector<AvailableLevel> {
    auto loaded{
        levels::authoring::DefinitionReader{}.read_root_file(scripts_directory / "catalog.scm")};
    auto const directory{path_to_utf8(scripts_directory)};
    menu_.levels.clear();
    if (!loaded) {
        host::present_catalog({directory, levels::format_diagnostics(loaded.error()), {}, {}});
        return {};
    }

    auto const rows{bridge::make_catalog_rows(
        *loaded, profiles_.active_records(), settings_.application().unlock_all_levels)};
    auto append = [&](presentation::LevelListing const& row) {
        auto const& id{row.metadata ? row.metadata->id.value : row.path};
        menu_.levels.push_back({id, row.category, row.unlocked});
    };
    for (auto const* campaign : rows.campaigns) {
        for (auto const& id : campaign->level_ids) {
            auto const it{std::ranges::find_if(rows.listings, [&](auto const& row) {
                return row.metadata && row.metadata->id == id;
            })};
            if (it != rows.listings.end()) {
                append(*it);
            }
        }
    }
    for (auto const& row : rows.listings) {
        if (!row.metadata ||
            !std::ranges::contains(menu_.levels, row.metadata->id.value, &CatalogControl::id)) {
            append(row);
        }
    }
    auto visible{[this](auto const& row) { return row.visible_in(menu_.category); }};
    auto const selected{std::ranges::find(menu_.levels, menu_.selected_level, &CatalogControl::id)};
    if (selected == menu_.levels.end() || !visible(*selected)) {
        auto const first{std::ranges::find_if(menu_.levels, visible)};
        menu_.selected_level = first == menu_.levels.end() ? std::string{} : first->id;
        if (menu_.focus.kind == MenuTarget::Kind::Level) {
            menu_.select(menu_.primary_target(settings_.view()));
        }
    }
    host::present_catalog({directory, rows.errors, rows.listings, rows.campaigns});

    std::vector<AvailableLevel> available_levels;
    available_levels.reserve(
        std::ranges::count(rows.listings, true, &presentation::LevelListing::unlocked));
    for (auto [entry, row] : std::views::zip(loaded->levels, rows.listings)) {
        if (row.unlocked) {
            available_levels.emplace_back(std::move(*entry.definition),
                                          std::move(entry.source_path));
        }
    }
    return available_levels;
}

void Application::publish_profiles() {
    if (!std::ranges::contains(profile_browser_.summaries(),
                               menu_.selected_profile,
                               &profiles::ProfileSummary::profile_id)) {
        menu_.selected_profile = profile_browser_.active_profile_id();
    }
    if (!menu_.selected_profile.empty()) {
        profile_browser_.load_profile_report(profiles_, menu_.selected_profile);
    }
    presentation::ProfileView view{
        profile_browser_.active_profile_id(), profile_browser_.summaries(), {}};
    if (auto const* report{profile_browser_.loaded_profile_report()}) {
        view.report.emplace(report->profile_id, report->outcomes);
    }
    menu_.profiles.clear();
    for (auto const& profile : view.summaries) {
        menu_.profiles.push_back(profile.profile_id);
    }
    menu_.active_profile = view.active_profile_id;
    if (!std::ranges::contains(menu_.profiles, menu_.selected_profile)) {
        menu_.selected_profile = menu_.active_profile;
    }
    menu_.outcomes.clear();
    if (view.report && view.report->profile_id == menu_.selected_profile) {
        for (auto const& row : view.report->outcomes) {
            menu_.outcomes.push_back(row.outcome_id);
        }
    }
    if (!std::ranges::contains(menu_.outcomes, menu_.selected_outcome)) {
        menu_.selected_outcome = menu_.outcomes.empty() ? std::string{} : menu_.outcomes.front();
    }
    endpoint_.present_profiles(view);
    endpoint_.exchange();
}

void Application::profile_action(MenuControl action) {
    using Result = profiles::CreateProfileResult;
    auto result{Result::succeeded};
    switch (action) {
        case MenuControl::ConfirmCreate:
            result = profiles_.create_profile(menu_.profile_name).result;
            if (result == Result::succeeded) {
                menu_.set_create_profile(false);
            }
            endpoint_.profile_result(result);
            break;
        case MenuControl::ActivateProfile:
            if (!profiles_.activate_profile(menu_.selected_profile)) {
                result = Result::persistence_failed;
            }
            break;
        case MenuControl::ResetProfile:
#if IOJ_SHIPPING
            return;
#else
            if (!profiles_.reset_test_profile(profiles::sample_profile_records())) {
                result = Result::persistence_failed;
            }
            break;
#endif
        case MenuControl::RefreshProfiles:
            break;
        default:
            std::unreachable();
    }
    if (result == Result::succeeded) {
        profile_browser_.refresh(profiles_);
        if (action != MenuControl::RefreshProfiles) {
            menu_.selected_profile = profile_browser_.active_profile_id();
        }
    } else if (result == Result::persistence_failed) {
        std::println(stderr, "Profile operation failed");
    }
    publish_profiles();
}

auto Application::process_preferences(Window& window, std::span<host::PreferenceEdit const> edits)
    -> bool {
    bool controls_changed{};
    auto const count{edits.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& edit{edits[index]};
        if (edit.action != host::PreferenceAction::Save) {
            auto result{
                edit.action == host::PreferenceAction::PreviewDisplay
                    ? window.preview({edit.values.video.window_mode, edit.values.video.resolution})
                    : window.revert_preview()};
            menu_.saving = false;
            if (result && edit.action == host::PreferenceAction::RevertDisplay) {
                menu_.draft.cancel();
                menu_.set_prompt(OptionsPrompt::None);
            } else if (result || menu_.prompt == OptionsPrompt::Display) {
                menu_.display_deadline =
                    std::chrono::steady_clock::now() + std::chrono::seconds{15};
                menu_.set_prompt(OptionsPrompt::Display);
            }
            host::preference_result(index, result ? std::string_view{} : result.error());
            continue;
        }

        auto result{edit.category == GameSettingCategory::Video ? window.verify_preview()
                                                                : ApplicationResult{}};
        if (result) {
            result = settings_.save_category(edit.category, edit.values, edit.original_settings);
        }
        menu_.saving = false;
        menu_.set_prompt(OptionsPrompt::None);
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
                settings_.apply(edit.values.video);
                window.keep_preview(settings_);
                break;
            case GameSettingCategory::Audio:
                settings_.apply(edit.values.audio);
                break;
            case GameSettingCategory::Controls:
                settings_.apply(edit.values.controls);
                controls_changed = true;
                break;
            case GameSettingCategory::Gameplay:
            case GameSettingCategory::Accessibility:
                break;
        }

        host::present_settings(settings_.view(), window.supported_resolutions());
        menu_.draft.begin(settings_.view().applied_values, settings_.view().defaults);
        host::preference_result(index, {});
    }
    return controls_changed;
}

auto Application::launch_level(LevelStorage& storage, std::filesystem::path const& path)
    -> ApplicationResult {
    auto definition{levels::authoring::DefinitionReader::for_level(path).read_level_file(path)};
    if (!definition) {
        return ApplicationResult{std::unexpect, levels::format_diagnostics(definition.error())};
    }

    levels::LaunchOptions options;
    options.control_context =
        definition->camera() ? levels::ControlContext::Observer : levels::ControlContext::Player;
    return launch_level(storage, *definition, path_to_utf8(path), options);
}
auto Application::launch_level(LevelStorage& storage,
                               levels::LevelDefinition const& definition,
                               std::string source_path,
                               levels::LaunchOptions options) -> ApplicationResult {
    if (settings_.application().start_levels_paused) {
        options.launch_mode = levels::LaunchMode::Paused;
    }

    sim::LevelRunTiming const timing{
        .time_scale = PositiveFinite<double>::make(options.requested_time_scale.get()),
        .duration = options.simulated_duration_seconds.transform(
            [](auto value) { return Duration::make(value); }),
    };
    auto built{sim::make_level_simulation_init_data(definition, {}, timing)};
    if (!built) {
        std::string errors;
        for (auto const& message : built.error()) {
            errors += message + "\n";
        }

        return ApplicationResult{std::unexpect, std::move(errors)};
    }

    storage.load(
        std::move(*built),
        bridge::make_level_presentation_setup(definition, std::move(source_path), options));
    menu_.open(MenuScreen::Closed);
    endpoint_.send(host::NativeMessageKind::OpenLevel);
    endpoint_.exchange();
    return {};
}
void Application::unload_level(LevelStorage& storage, sim::RunEndReason reason, bool show_results) {
    auto* const level{storage.get()};
    if (!level) {
        return;
    }

    auto const result{level->simulation.stop_run(reason)};
    auto const completed{bridge::completed_level(level->simulation, result)};
    auto const score{profiles::make_level_score(completed, profiles::current_timestamp())};
    profiles::RecordSaveResult saved{};
    if (score) {
        saved = profiles_.append_score_record(*score);
        if (saved.persisted) {
            profile_browser_.refresh(profiles_);
            publish_profiles();
        } else {
            std::println(stderr, "Failed to save the score for {}", completed.metadata.id.value);
        }
    }

    if (engine_.presents_level) {
        if (show_results) {
            menu_.open(MenuScreen::Completion);
        }
        endpoint_.finish_level({completed, score.has_value(), saved.new_best_time, show_results});
        endpoint_.exchange();
    }

    storage.unload();
}
}
