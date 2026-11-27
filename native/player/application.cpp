#include "player/application.h"

#include "player/cli_args.h"
#include "player/engine.h"
#include "player/game_input.h"
#include "player/loaded_level.h"
#include "player/sdl_input.h"
#include "player/window.h"
#include <ioj/bridge/completed_level.h>
#include <ioj/bridge/level_catalog.h>
#include <ioj/bridge/level_frame.h>
#include <ioj/files.h>
#include <ioj/host/game_host_api.h>
#include <ioj/host/level_host.h>
#include <ioj/input/menu_navigation.h>
#include <ioj/input/ship_input.h>
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
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>

#include <algorithm>
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

auto initialize_presentation(LoadedLevel& level) -> bool {
    auto const mode{level.presentation_setup->launch_mode};
    auto& simulation{level.simulation};
    auto const initialized{
        host::initialize_level(bridge::level_init(*level.presentation_setup, simulation),
                               bridge::level_frame(simulation))};
    level.presentation_setup.reset();

    if (initialized && mode == levels::LaunchMode::Running &&
        simulation.state() == sim::LevelState::Paused) {
        simulation.start();
    }
    return initialized;
}
auto level_input_route(LoadedLevel const& level) -> InputRoute {
    switch (level.control_context) {
        case levels::ControlContext::Observer:
            return {InputContext::Observer};
        case levels::ControlContext::Benchmark:
            return {InputContext::Benchmark};
        case levels::ControlContext::Player: {
            auto const* ship{level.simulation.player_ship_simulation()};
            if (ship && ship->is_alive(level.simulation.entity_tables().health)) {
                return {InputContext::Gameplay};
            }
            break;
        }
        case levels::ControlContext::None:
            break;
    }
    return {};
}
void advance_level(sim::LevelSim& simulation, double delta_seconds) {
    auto const state{simulation.state()};
    simulation.advance(delta_seconds);
    auto const frame{bridge::level_frame(simulation)};
    auto const visual_delta{simulation.state() == sim::LevelState::Running ||
                                    !frame.events.ticks.empty()
                                ? delta_seconds
                                : 0.0};
    host::present_level(visual_delta, frame);

    if (state == sim::LevelState::Running && simulation.state() == sim::LevelState::Paused) {
        host::simulation_paused();
    }
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
    IojPlayer_Attach(capabilities);
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
namespace {
struct PlayerInputSession {
    PlayerInputSession(SDL_Window& window,
                       ControlsSettings const& controls,
                       LevelStorage& levels,
                       std::chrono::steady_clock::time_point& previous_frame)
        : storage{levels}
        , previous_frame_time{previous_frame}
        , devices{window}
        , input{controls, devices.focused() ? WindowFocus::Gained : WindowFocus::Lost} {}

    void synchronize_input() {
        auto const menu{host::menu_status()};
        auto const* level{storage.get()};
        auto const route{level && !level->presentation_setup
                             ? level->pause_menu.route(level_input_route(*level))
                             : InputRoute{}};
        input.synchronize(commands, route, menu);
        devices.set_window_modes(input.relative_mouse(), input.editing_text());
    }
    void update_pause_simulation(LoadedLevel& level) {
        auto const request{level.pause_menu.simulation_request(level.simulation.state())};
        if (request == levels::LaunchMode::Paused) {
            level.simulation.pause();
        } else if (request == levels::LaunchMode::Running) {
            level.simulation.start();
            previous_frame_time = std::chrono::steady_clock::now();
        }
    }
    void present_pause(LoadedLevel& level, PauseMenu::Command command) {
        update_pause_simulation(level);
        if (command && !host::present_pause(*command)) {
            pause_presentation =
                std::unexpected{"Could not present the native pause menu. See the game log."};
        }
    }
    void dispatch(host::PlayerAction action) {
        auto* const level{storage.get()};
        if (!level || level->presentation_setup || end_benchmark) {
            return;
        }
        auto& simulation{level->simulation};
        using enum host::PlayerAction;
        if (action == BenchmarkActivate) {
            action = end_selected ? EndBenchmark : ToggleBenchmark;
        }
        switch (action) {
            case OpenPause:
            case RequestResume:
                present_pause(*level, level->pause_menu.handle(action, simulation.state()));
                break;
            case BenchmarkNext:
                end_selected = !end_selected;
                host::select_benchmark_button(end_selected);
                break;
            case ToggleBenchmark:
                if (simulation.state() == sim::LevelState::Running) {
                    simulation.pause();
                } else if (simulation.state() == sim::LevelState::Paused) {
                    simulation.start();
                    previous_frame_time = std::chrono::steady_clock::now();
                }
                break;
            case EndBenchmark:
                end_benchmark = true;
                break;
            case BenchmarkActivate:
                break;
        }
    }
    void process_widget_actions() {
        for (;;) {
            auto const actions{host::take_widget_actions()};
            if (actions.empty()) {
                break;
            }
            for (auto const& action : actions) {
                if (auto const* item{std::get_if<MainMenuItem>(&action)}) {
                    host::activate_main_item(*item);
                } else if (auto const* event{std::get_if<host::PauseWidgetEvent>(&action)}) {
                    if (auto* level{storage.get()}) {
                        present_pause(*level, level->pause_menu.widget_event(*event));
                    }
                } else {
                    dispatch(std::get<host::PlayerAction>(action));
                }
            }
        }
    }
    void consume_input() {
        for (auto const& event : commands) {
            if (auto const* action{std::get_if<host::PlayerAction>(&event)}) {
                dispatch(*action);
            } else if (auto const* button{std::get_if<host::MenuButton>(&event)}) {
                auto const focus{host::menu_status().main_focus};
                using enum MenuCommand;
                auto* level{storage.get()};
                if (level && level->pause_menu.view == PauseMenu::View::Sidebar &&
                    !level->pause_menu.options_modal) {
                    if (button->down) {
                        present_pause(*level,
                                      level->pause_menu.navigate(button->command, button->repeat));
                    }
                } else if (focus && (button->command == Up || button->command == Down)) {
                    if (button->down) {
                        host::focus_main_item(move_main_selection(*focus, button->command));
                    }
                } else if (focus && button->command == Accept) {
                    if (button->down && !button->repeat) {
                        host::activate_main_item(*focus);
                    }
                } else {
                    host::present_menu_button(*button);
                }
            } else {
                host::present_input(std::get<host::PresentationEvent>(event));
            }
            process_widget_actions();
        }
        commands.clear();
    }

    LevelStorage& storage;
    std::chrono::steady_clock::time_point& previous_frame_time;
    SdlInput devices;
    GameInput input;
    host::InputEvents commands;
    ApplicationResult pause_presentation{};
    bool end_selected{};
    bool end_benchmark{};
};
}

auto Application::play(Window& window,
                       LevelStorage& storage,
                       std::filesystem::path const& level_path,
                       std::filesystem::path const& scripts_directory,
                       bool exit_when_ready) -> ApplicationResult {
    if (!IojPlayer_GameReady()) {
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
    auto previous_frame_time{std::chrono::steady_clock::now()};
    auto const deadline{previous_frame_time + std::chrono::minutes{2}};
    std::vector<AvailableLevel> available_levels;

    PlayerInputSession play_input{
        *window.sdl_window(), settings_.applied().controls, storage, previous_frame_time};
    std::vector<sim::player::PlayerInputKind> ship_events;
    auto last_input_frame{previous_frame_time};
    while (!IojPlayer_ExitRequested()) {
        IojPlayer_BeginFrame();
        play_input.input.begin_frame();
        play_input.synchronize_input();
        play_input.consume_input();
        SDL_Event event;
        bool close_requested{};
        bool viewport_changed{};
        while (SDL_PollEvent(&event)) {
            if (auto physical{play_input.devices.translate(event)}) {
                play_input.input.resolve(play_input.commands, *physical);
                play_input.consume_input();
            }
            play_input.synchronize_input();
            play_input.consume_input();
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
                    host::present_settings(settings_.view(), window.supported_resolutions());
                    break;
            }
        }
        if (!play_input.pause_presentation) {
            return play_input.pause_presentation;
        }
        if (close_requested || IojPlayer_ExitRequested()) {
            break;
        }

        if (viewport_changed) {
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

        auto const input_time{std::chrono::steady_clock::now()};
        auto const input_delta{std::chrono::duration<double>{input_time - last_input_frame}};
        last_input_frame = input_time;
        play_input.synchronize_input();
        play_input.consume_input();
        auto const seconds{std::min(input_delta.count(), max_frame_delta_seconds)};
        if (play_input.devices.focused()) {
            play_input.input.repeat_navigation(play_input.commands, seconds);
        }
        play_input.input.append_frame_commands(seconds, play_input.commands);
        play_input.consume_input();
        if (auto result{tick_engine()}; !result) {
            return result;
        }

        play_input.process_widget_actions();
        auto const messages{host::take_messages()};
        if (play_input.end_benchmark) {
            auto const id{storage.get()->simulation.level_metadata().id};
            unload_level(storage, sim::RunEndReason::WorldEnd, false);
            host::open_menu(id.value, {});
            play_input.end_benchmark = false;
            continue;
        }
        process_preferences(
            window, play_input.input, play_input.commands, messages.preference_edits);
        process_profile_actions(messages.profile_actions);

        if (messages.refresh_catalog) {
            available_levels = publish_catalog(scripts_directory);
        }

        auto const& navigation{messages.navigation_request};
        if (exit_when_ready && !std::holds_alternative<std::monostate>(navigation)) {
            return ApplicationResult{std::unexpect, "Level startup was cancelled"};
        }
        if (auto const* menu{std::get_if<session::LevelSelectRequest>(&navigation)}) {
            unload_level(storage, sim::RunEndReason::WorldEnd, false);
            host::open_menu(menu->preferred_level_id.value, menu->launch_error);
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
                    host::open_menu(selection->id.value, result.error());
                }
                continue;
            }
            host::open_menu(selection->id.value, "The selected level is no longer available.");
            continue;
        }

        auto* const level{storage.get()};
        if (!level) {
            continue;
        }

        if (level->presentation_setup && host::level_world_ready()) {
            play_input.end_selected = false;
            auto const start_paused{level->presentation_setup->launch_mode ==
                                        levels::LaunchMode::Paused &&
                                    (level->control_context == levels::ControlContext::Player ||
                                     level->control_context == levels::ControlContext::Observer)};
            if (!initialize_presentation(*level)) {
                unload_level(storage, sim::RunEndReason::WorldEnd, false);
                if (exit_when_ready) {
                    return ApplicationResult{std::unexpect,
                                             "Level initialization failed. See the game log."};
                }
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

            previous_frame_time = std::chrono::steady_clock::now();
            continue;
        }

        if (host::presents_level()) {
            level->simulation.append_player_input(
                ship_input_frame(play_input.input.state(), ship_events));

            auto const frame_time{std::chrono::steady_clock::now()};
            if (messages.requested_simulation_mode == levels::LaunchMode::Running &&
                level->simulation.state() == sim::LevelState::Paused) {
                level->simulation.start();
                previous_frame_time = frame_time;
            } else if (messages.requested_simulation_mode == levels::LaunchMode::Paused &&
                       level->simulation.state() == sim::LevelState::Running) {
                level->simulation.pause();
            }

            auto const elapsed{std::chrono::duration<double>{frame_time - previous_frame_time}};
            previous_frame_time = frame_time;
            advance_level(level->simulation, std::min(elapsed.count(), max_frame_delta_seconds));
            play_input.update_pause_simulation(*level);
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

auto Application::publish_catalog(std::filesystem::path const& scripts_directory) const
    -> std::vector<AvailableLevel> {
    auto loaded{
        levels::authoring::DefinitionReader{}.read_root_file(scripts_directory / "catalog.scm")};
    auto const directory{path_to_utf8(scripts_directory)};
    if (!loaded) {
        host::present_catalog({directory, levels::format_diagnostics(loaded.error()), {}, {}});
        return {};
    }

    auto const rows{bridge::make_catalog_rows(
        *loaded, profiles_.active_records(), settings_.application().unlock_all_levels)};
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

void Application::publish_profiles() const {
    presentation::ProfileView view{
        profile_browser_.active_profile_id(), profile_browser_.summaries(), {}};
    if (auto const* report{profile_browser_.loaded_profile_report()}) {
        view.report.emplace(report->profile_id, report->outcomes);
    }
    host::present_profiles(view);
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

        publish_profiles();
        host::profile_result(index, result);
    }
}

void Application::process_preferences(Window& window,
                                      GameInput& input,
                                      host::InputEvents& commands,
                                      std::span<host::PreferenceEdit const> edits) {
    auto const count{edits.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const& edit{edits[index]};
        if (edit.action != host::PreferenceAction::Save) {
            auto result{
                edit.action == host::PreferenceAction::PreviewDisplay
                    ? window.preview({edit.values.video.window_mode, edit.values.video.resolution})
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
                settings_.apply(edit.values.video);
                window.keep_preview(settings_);
                break;
            case GameSettingCategory::Audio:
                settings_.apply(edit.values.audio);
                break;
            case GameSettingCategory::Controls:
                settings_.apply(edit.values.controls);
                input.refresh_bindings(commands);
                break;
            case GameSettingCategory::Gameplay:
            case GameSettingCategory::Accessibility:
                break;
        }

        host::present_settings(settings_.view(), window.supported_resolutions());
        host::preference_result(index, {});
    }
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
    host::open_level();
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

    if (host::presents_level()) {
        host::finish_level(completed, score.has_value(), saved.new_best_time, show_results);
    }

    storage.unload();
}
}
