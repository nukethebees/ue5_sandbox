#include "player/application.h"

#include "player/engine.h"
#include "player/input/input_system.h"
#include "player/loaded_level.h"
#include "player/window.h"
#include <ioj/bridge/completed_level.h>
#include <ioj/bridge/level_catalog.h>
#include <ioj/bridge/level_frame.h>
#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/menu/player_menu.h>
#include <ioj/presentation/level_catalog.h>
#include <ioj/presentation/profile_view.h>
#include <ioj/profiles/level_score.h>
#include <ioj/profiles/sample_profile_records.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <ioj/system_capabilities.h>

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

namespace ioj {
/* **************************************** */
// Process lifecycle
/* **************************************** */
Application::Application(Window window,
                         PlayerSettings settings,
                         profiles::ProfileManager profiles,
                         std::filesystem::path scripts_directory,
                         bool exit_when_ready)
    : window_{std::move(window)}
    , settings_{std::move(settings)}
    , menu_{!IOJ_SHIPPING}
    , profiles_{std::move(profiles)}
    , input_{*window_.sdl_window(), settings_.applied().controls, endpoint_}
    , scripts_directory_{std::move(scripts_directory)}
    , exit_when_ready_{exit_when_ready} {
    profile_browser_.refresh(profiles_);
    menu_.draft.begin(settings_.applied(), settings_.defaults());
}

auto Application::run(std::wstring const& engine_command_line,
                      std::filesystem::path const& level_path) -> ApplicationResult {
    auto const capabilities{query_system_capabilities()};
    endpoint_.present_capabilities(capabilities);
    endpoint_.present_keyboard(input_.keyboard());
    endpoint_.present_menu(menu_);
    publish_settings();
    endpoint_.publish();
    if (auto result{start_engine(window_, engine_command_line, settings_.launch_display_mode())};
        !result) {
        return result;
    }

    ApplicationResult play_result;
    try {
        play_result = play(level_path);
    } catch (std::exception const& error) {
        play_result = std::unexpected{std::string{error.what()}};
    } catch (...) {
        play_result = std::unexpected{"Unexpected exception while running the game"};
    }

    try {
        unload_level(sim::RunEndReason::WorldEnd, false);
        consume_replies();
        if (play_result && !result_) {
            play_result = result_;
        }
    } catch (...) {
        if (auto shutdown{shutdown_engine(window_)}; !shutdown) {
            std::println(
                stderr, "Engine shutdown failed during level cleanup: {}", shutdown.error());
        }

        throw;
    }

    auto shutdown_result{shutdown_engine(window_)};
    if (!play_result && !shutdown_result) {
        play_result.error() += "; " + shutdown_result.error();
    }

    if (!play_result) {
        return play_result;
    }

    return shutdown_result;
}
/* **************************************** */
// Endpoint replies
/* **************************************** */
void Application::consume_replies() {
    // Publishing a borrowed frame or catalog can append replies and renew these spans.
    auto const& incoming{endpoint_.publish()};
    for (std::size_t cursor{}; cursor < incoming.order.size(); ++cursor) {
        auto const [kind, index]{incoming.order[cursor]};
        using enum host::UnrealMessageKind;
        switch (kind) {
            case Status:
                engine_ = incoming.status[index];
                if (engine_.failed) {
                    result_ = std::unexpected{"Unreal reported a critical error"};
                }
                break;
            case Hit:
                if (auto update{menu_.activate(
                        incoming.hits[index], settings_.view(), window_.supported_resolutions())}) {
                    apply_menu(std::move(*update));
                }
                break;
            case Action:
                handle_action(incoming.actions[index]);
                break;
            case PauseInteraction: {
                auto const event{incoming.pause_events[index]};
                if (menu_.screen != MenuScreen::Pause) {
                    break;
                }
                switch (event.kind) {
                    case host::PauseWidgetEventKind::Activate:
                        if (!menu_.pause_blocked()) {
                            choose_pause(event.item);
                        }
                        break;
                    case host::PauseWidgetEventKind::Select:
                        if (menu_.select_pause_item(event.item)) {
                            endpoint_.present_pause({host::PauseCommandKind::Select, event.item});
                        }
                        break;
                    case host::PauseWidgetEventKind::ContentFocused:
                        if (!menu_.pause_blocked()) {
                            menu_.pause_sidebar = false;
                        }
                        break;
                }
                break;
            }
            case Setting: {
                auto const& change{incoming.settings[index]};
                if (!menu_.pause_blocked() &&
                    settings_.view().set_setting(menu_.draft,
                                                 change.setting,
                                                 change.value,
                                                 window_.supported_resolutions())) {
                    menu_.select(change.setting);
                    present_menu({menu_.focus, MenuOperation::RefreshSettings});
                }
                break;
            }
            case EditText: {
                auto const& edit{incoming.edits[index]};
                if (auto update{menu_.edit_text(edit.control, edit.value)}) {
                    present_menu(std::move(*update));
                }
                break;
            }
            case RefreshCatalog:
                publish_catalog();
                break;
            case RefreshProfiles:
                profile_action(MenuControl::RefreshProfiles);
                break;
            case ResetTestProfile:
                profile_action(MenuControl::ResetProfile);
                break;
            case SimulationMode:
                if (auto* level{storage_.get()};
                    level && !level->presentation_setup && menu_.screen != MenuScreen::Pause) {
                    level->set_simulation_mode(incoming.simulation_modes[index]);
                }
                break;
            case Applied:
                finish_preferences(incoming.settings_results[index]);
                break;
            case LevelInitialized: {
                auto* level{storage_.get()};
                if (!level || !level->presentation_setup) {
                    break;
                }
                auto const& error{incoming.level_results[index]};
                auto const mode{level->presentation_setup->launch_mode};
                level->presentation_setup.reset();
                if (!error.empty()) {
                    return_to_menu(error);
                    break;
                }
                if (mode == levels::LaunchMode::Running) {
                    level->set_simulation_mode(mode);
                } else if (level->control_context == levels::ControlContext::Player ||
                           level->control_context == levels::ControlContext::Observer) {
                    handle_action(host::PlayerAction::OpenPause);
                }
                level->previous_frame = std::chrono::steady_clock::now();
                break;
            }
            case PausePresented:
                if (!incoming.pause_results[index]) {
                    result_ =
                        std::unexpected{"Could not present the pause menu. See the game log."};
                }
                break;
        }
        if (endpoint_.size() != 0) {
            endpoint_.publish();
        }
    }
    endpoint_.release_replies();
}
auto Application::route() const -> InputRoute {
    auto const* level{storage_.get()};
    if (menu_.screen != MenuScreen::Closed || !level) {
        return {InputContext::Menu, menu_.screen == MenuScreen::Pause && !menu_.pause_blocked()};
    }
    return {level->input_context()};
}
void Application::present_menu(MenuUpdate update) {
    endpoint_.present_menu(menu_, std::move(update));
}
void Application::handle_action(host::PlayerAction action) {
    auto* level{storage_.get()};
    if (!level || level->presentation_setup) {
        return;
    }
    using enum host::PlayerAction;
    switch (action) {
        case OpenPause:
            if (menu_.screen != MenuScreen::Closed ||
                level->simulation.state() == sim::LevelState::Stopped) {
                return;
            }
            menu_.open(MenuScreen::Pause);
            level->update_pause(true);
            endpoint_.present_menu(menu_);
            endpoint_.present_pause({host::PauseCommandKind::Open, host::PauseMenuItem::Resume});
            break;
        case RequestResume:
            if (menu_.screen == MenuScreen::Pause && !menu_.pause_blocked()) {
                choose_pause(host::PauseMenuItem::Resume);
            }
            break;
        case BenchmarkNext:
            level->end_selected = !level->end_selected;
            endpoint_.select_benchmark(level->end_selected);
            break;
        case BenchmarkActivate:
            handle_action(level->end_selected ? EndBenchmark : ToggleBenchmark);
            break;
        case ToggleBenchmark:
            level->set_simulation_mode(level->simulation.state() == sim::LevelState::Running
                                           ? levels::LaunchMode::Paused
                                           : levels::LaunchMode::Running);
            break;
        case EndBenchmark:
            return_to_menu();
            break;
    }
}
void Application::choose_pause(host::PauseMenuItem item) {
    if (item != menu_.pause_item() && !menu_.leave_ready()) {
        menu_.pending_pause = item;
        present_menu({menu_.focus, MenuOperation::Focus});
        return;
    }
    using enum host::PauseMenuItem;
    switch (item) {
        case Resume:
            menu_.open(MenuScreen::Closed);
            storage_.get()->update_pause(false);
            endpoint_.present_menu(menu_);
            endpoint_.present_pause({host::PauseCommandKind::Finish, item});
            break;
        case Controls:
        case Audio:
            menu_.select_page(item == Audio ? MainMenuItem::Audio : MainMenuItem::Controls);
            menu_.pause_sidebar = false;
            menu_.select(menu_.primary_target(settings_.view()));
            endpoint_.present_menu(menu_);
            endpoint_.present_pause({host::PauseCommandKind::ShowPage, item});
            break;
        case LevelSelect:
            return_to_menu();
            break;
        case Quit:
            quit_requested_ = true;
            break;
    }
}
void Application::finish_leave(bool accepted) {
    if (auto item{std::exchange(menu_.pending_pause, {})}; item && accepted) {
        choose_pause(*item);
    } else if (auto update{menu_.finish_main_leave(accepted)}) {
        apply_menu(std::move(*update));
    }
}
void Application::consume_commands() {
    for (std::size_t i{}; i < input_.commands().size(); ++i) {
        auto const command{input_.commands()[i]};
        switch (command.kind) {
            case InputCommand::Kind::Action:
                handle_action(command.action);
                break;
            case InputCommand::Kind::Capture:
                menu_.draft.capture_binding(command.key);
                present_menu({menu_.draft.binding_edit().mapping, MenuOperation::BindingPrompt});
                break;
            case InputCommand::Kind::Navigation: {
                auto const button{command.button};
                if (menu_.draft.binding_edit().prompt != BindingPrompt::Closed) {
                    menu_.draft.navigate_binding(button.command, button.repeat);
                    present_menu(
                        {menu_.draft.binding_edit().mapping, MenuOperation::BindingPrompt});
                } else if (menu_.screen == MenuScreen::Pause &&
                           (menu_.pause_sidebar || button.command == MenuCommand::Back) &&
                           !menu_.pause_blocked()) {
                    auto const before{menu_.pause_selection};
                    if (auto item{menu_.navigate_pause(button.command, button.repeat)}) {
                        choose_pause(*item);
                    } else if (before != menu_.pause_selection) {
                        endpoint_.present_pause(
                            {host::PauseCommandKind::Select, menu_.pause_selection});
                    }
                } else if (auto update{menu_.navigate(button.command,
                                                      button.repeat,
                                                      settings_.view(),
                                                      window_.supported_resolutions())}) {
                    apply_menu(std::move(*update));
                }
                break;
            }
        }
    }
    input_.clear_commands();
}
void Application::process_input(double seconds) {
    auto const window{input_.poll()};
    if (window.close_requested) {
        quit_requested_ = true;
        return;
    }
    if (window.keyboard_changed) {
        endpoint_.present_keyboard(input_.keyboard());
    }
    if (window.display_changed) {
        result_ = window_.refresh_resolutions();
        if (!result_) {
            return;
        }
        publish_settings();
    }
    if (window.viewport_changed) {
        result_ = window_.synchronize_resize(settings_);
        if (!result_) {
            return;
        }
        publish_settings();
    }
    auto const& events{input_.events()};
    std::size_t cursor{};
    for (;;) {
        input_.synchronize(route(), menu_.input_mode());
        if (!input_.commands().empty()) {
            consume_commands();
            continue;
        }
        if (cursor == events.order.size()) {
            break;
        }
        cursor = input_.map_events(events, cursor);
        consume_commands();
        if (endpoint_.size() != 0) {
            consume_replies();
        }
        if (quit_requested_ || !result_) {
            return;
        }
    }
    input_.repeat_navigation(seconds);
    consume_commands();
    input_.publish_viewer_step(seconds);
    if (route().context == InputContext::Gameplay) {
        endpoint_.present_steering(input_.steering());
    }
    if (endpoint_.size() != 0) {
        consume_replies();
    }
}

void Application::apply_menu(MenuUpdate update) {
    if (update.operation != MenuOperation::Activate) {
        present_menu(std::move(update));
        return;
    }
    auto const& target{update.target};
    if (target.kind == MenuTarget::Kind::Page && target.page() == MainMenuItem::Quit) {
        quit_requested_ = true;
        return;
    }
    if (target.kind == MenuTarget::Kind::Profile || target.kind == MenuTarget::Kind::Outcome) {
        publish_profiles();
    }
    if (target.kind != MenuTarget::Kind::Control) {
        present_menu(std::move(update));
        return;
    }
    using enum MenuControl;
    switch (target.control()) {
        case ReturnToLevels:
            return_to_menu();
            return;
        case Launch: {
            auto const entry{std::ranges::find_if(available_levels_, [this](auto const& candidate) {
                return candidate.definition.metadata().id.value == menu_.selected_level;
            })};
            auto const options{menu_.launch_options()};
            if (!options) {
                return;
            }
            if (entry == available_levels_.end()) {
                return_to_menu("The selected level is no longer available.");
                return;
            }
            auto result{
                launch_level(entry->definition, path_to_utf8(entry->source_path), *options)};
            if (!result) {
                return_to_menu(result.error());
            }
            available_levels_.clear();
            return;
        }
        case Save:
        case Apply:
            apply_preferences(false);
            return;
        case KeepDisplay:
            apply_preferences(true);
            return;
        case RevertDisplay:
            revert_display();
            return;
        case Stay:
        case Discard: {
            bool const discard{target.control() == Discard};
            menu_.set_prompt(OptionsPrompt::None);
            if (discard) {
                menu_.draft.cancel();
            }
            present_menu({menu_.focus, MenuOperation::Focus});
            finish_leave(discard);
            return;
        }
        case ConfirmCreate:
        case ActivateProfile:
        case RefreshProfiles:
        case ResetProfile:
            profile_action(target.control());
            return;
        default:
            present_menu(std::move(update));
            break;
    }
}
void Application::apply_preferences(bool keep_display) {
    auto const category{settings_category(menu_.page)};
    if (!category || menu_.saving || !menu_.draft.is_dirty()) {
        return;
    }
    auto const& values{menu_.draft.pending()};
    auto const& applied{menu_.draft.applied()};
    if (!keep_display && *category == GameSettingCategory::Video &&
        (values.video.window_mode != applied.video.window_mode ||
         values.video.resolution != applied.video.resolution)) {
        auto result{window_.preview({values.video.window_mode, values.video.resolution})};
        if (!result) {
            endpoint_.show_error(result.error());
            finish_leave(false);
        } else {
            menu_.display_deadline = std::chrono::steady_clock::now() + std::chrono::seconds{15};
            menu_.set_prompt(OptionsPrompt::Display);
        }
        present_menu({menu_.focus, MenuOperation::Focus});
        return;
    }
    menu_.saving = true;
    endpoint_.present_menu(menu_);
    if (*category == GameSettingCategory::Video || *category == GameSettingCategory::Audio) {
        endpoint_.apply_settings({host::PreferenceOperation::Save, *category, values});
    } else {
        finish_preferences({host::PreferenceOperation::Save, *category, {}});
    }
}
void Application::revert_display() {
    auto result{window_.revert_preview()};
    if (result) {
        menu_.draft.cancel();
        menu_.set_prompt(OptionsPrompt::None);
        finish_leave(false);
    } else {
        endpoint_.show_error(result.error());
        menu_.display_deadline = std::chrono::steady_clock::now() + std::chrono::seconds{15};
    }
    present_menu({menu_.focus, MenuOperation::RefreshSettings});
}
void Application::finish_preferences(host::SettingsApplied reply) {
    if (reply.operation == host::PreferenceOperation::Startup) {
        if (!reply.error.empty()) {
            result_ = std::unexpected{std::move(reply.error)};
        }
        return;
    }
    if (reply.operation == host::PreferenceOperation::Restore) {
        if (!reply.error.empty()) {
            endpoint_.show_error(std::move(reply.error));
        }
        return;
    }
    auto const category{reply.category};
    auto const& values{menu_.draft.pending()};
    auto const& original{menu_.draft.applied()};
    ApplicationResult result;
    if (!reply.error.empty()) {
        result = std::unexpected{std::move(reply.error)};
    }
    if (result && category == GameSettingCategory::Video) {
        result = window_.verify_preview();
    }
    if (result) {
        result = settings_.save_category(category, values, original);
    }
    menu_.saving = false;
    menu_.set_prompt(OptionsPrompt::None);
    if (!result) {
        if (category == GameSettingCategory::Video) {
            if (auto rollback{window_.revert_preview()}; !rollback) {
                result.error() += "; rollback failed: " + rollback.error();
            }
        }
        if (category == GameSettingCategory::Video || category == GameSettingCategory::Audio) {
            endpoint_.apply_settings({host::PreferenceOperation::Restore, category, original});
        }
        endpoint_.show_error(result.error());
    } else {
        switch (category) {
            case GameSettingCategory::Video:
                settings_.apply(values.video);
                window_.keep_preview(settings_);
                break;
            case GameSettingCategory::Audio:
                settings_.apply(values.audio);
                break;
            case GameSettingCategory::Controls:
                settings_.apply(values.controls);
                input_.refresh_bindings();
                break;
            default:
                break;
        }
        publish_settings();
        menu_.draft.begin(settings_.view().applied_values, settings_.view().defaults);
    }
    present_menu({menu_.focus, MenuOperation::RefreshSettings});
    finish_leave(bool(result));
}
void Application::publish_settings() {
    endpoint_.present_settings({settings_.view(), window_.supported_resolutions()});
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
void Application::return_to_menu(std::string error) {
    if (exit_when_ready_) {
        result_ = std::unexpected{error.empty() ? "Level startup was cancelled" : error};
    }
    if (auto* level{storage_.get()}) {
        menu_.selected_level = level->simulation.level_metadata().id.value;
    }
    unload_level(sim::RunEndReason::WorldEnd, false);
    menu_.open(MenuScreen::Main, !menu_.selected_level.empty());
    endpoint_.present_menu(menu_);
    endpoint_.open_menu(std::move(error));
}
auto Application::play(std::filesystem::path const& level_path) -> ApplicationResult {
    consume_replies();
    if (!engine_.ready) {
        return std::unexpected{"Unreal did not initialize a playable world. See the game log."};
    }
    for (auto category : {GameSettingCategory::Video, GameSettingCategory::Audio}) {
        endpoint_.apply_settings(
            {host::PreferenceOperation::Startup, category, settings_.view().applied_values});
    }
    consume_replies();
    if (!result_) {
        return result_;
    }
    if (settings_.launch_display_mode() != LaunchDisplayMode::Headless) {
        if (auto result{window_.show(settings_)}; !result) {
            return result;
        }
        publish_settings();
    }
    if (!level_path.empty()) {
        if (auto result{launch_level(level_path)}; !result) {
            return result;
        }
    }
    consume_replies();
    auto previous{std::chrono::steady_clock::now()};
    auto const deadline{previous + std::chrono::minutes{2}};
    while (result_ && !quit_requested_ && !engine_.exit_requested) {
        auto const now{std::chrono::steady_clock::now()};
        if (menu_.prompt == OptionsPrompt::Display && !menu_.saving &&
            now >= menu_.display_deadline) {
            revert_display();
        }
        process_input(std::min(
            0.1, std::chrono::duration<double>{now - std::exchange(previous, now)}.count()));
        if (!result_ || quit_requested_ || engine_.exit_requested) {
            break;
        }
        if (exit_when_ready_) {
            if (level_path.empty()) {
                std::println("Ready: MainMenu");
                return {};
            }
            if (now >= deadline) {
                return std::unexpected{"Timed out waiting for level readiness. See the game log."};
            }
        }
        if (auto result{tick_engine()}; !result) {
            return result;
        }
        consume_replies();
        auto* level{storage_.get()};
        if (!level || !result_) {
            continue;
        }
        auto& simulation{level->simulation};
        if (level->presentation_setup && engine_.world_ready) {
            endpoint_.initialize_level({bridge::level_init(*level->presentation_setup, simulation),
                                        bridge::level_frame(simulation)});
            consume_replies();
            if (exit_when_ready_ && result_ && engine_.presents_level) {
                std::println("Ready: {}", path_to_utf8(level_path));
                return {};
            }
        } else if (engine_.presents_level && !level->presentation_setup) {
            simulation.append_player_input(input_.ship_input());
            auto const before{simulation.state()};
            auto const seconds{level->elapsed_seconds()};
            simulation.advance(seconds);
            auto frame{bridge::level_frame(simulation)};
            auto const visual_seconds{simulation.state() == sim::LevelState::Running ||
                                              !frame.events.ticks.empty()
                                          ? seconds
                                          : 0.0};
            endpoint_.present_frame({visual_seconds, frame});
            if (before == sim::LevelState::Running &&
                simulation.state() == sim::LevelState::Paused) {
                endpoint_.send(host::NativeMessageKind::SimulationPaused);
            }
            endpoint_.publish();
            level->update_pause(menu_.screen == MenuScreen::Pause);
        }
        if (auto* current{storage_.get()};
            current && !current->presentation_setup &&
            current->simulation.state() == sim::LevelState::Stopped) {
            unload_level(sim::RunEndReason::LevelCompleted, true);
        }
        consume_replies();
    }
    if (!result_) {
        return result_;
    }
    if (exit_when_ready_) {
        return std::unexpected{"Engine exited before readiness"};
    }
    return {};
}

void Application::publish_catalog() {
    auto const& scripts_directory{scripts_directory_};
    available_levels_.clear();
    auto loaded{
        levels::authoring::DefinitionReader{}.read_root_file(scripts_directory / "catalog.scm")};
    auto const directory{path_to_utf8(scripts_directory)};
    menu_.levels.clear();
    if (!loaded) {
        auto const error{levels::format_diagnostics(loaded.error())};
        endpoint_.present_catalog({directory, error, {}, {}});
        endpoint_.publish();
        return;
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
    endpoint_.present_menu(menu_);
    endpoint_.present_catalog({directory, rows.errors, rows.listings, rows.campaigns});
    endpoint_.publish();

    available_levels_.reserve(
        std::ranges::count(rows.listings, true, &presentation::LevelListing::unlocked));
    for (auto [entry, row] : std::views::zip(loaded->levels, rows.listings)) {
        if (row.unlocked) {
            available_levels_.emplace_back(std::move(*entry.definition),
                                           std::move(entry.source_path));
        }
    }
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
    endpoint_.present_menu(menu_);
    endpoint_.present_profiles(view);
    endpoint_.publish();
}

auto Application::launch_level(std::filesystem::path const& path) -> ApplicationResult {
    auto definition{levels::authoring::DefinitionReader::for_level(path).read_level_file(path)};
    if (!definition) {
        return ApplicationResult{std::unexpect, levels::format_diagnostics(definition.error())};
    }

    levels::LaunchOptions options;
    options.control_context =
        definition->camera() ? levels::ControlContext::Observer : levels::ControlContext::Player;
    return launch_level(*definition, path_to_utf8(path), options);
}
auto Application::launch_level(levels::LevelDefinition const& definition,
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

    storage_.load(
        std::move(*built),
        bridge::make_level_presentation_setup(definition, std::move(source_path), options));
    menu_.open(MenuScreen::Closed);
    endpoint_.present_menu(menu_);
    endpoint_.send(host::NativeMessageKind::OpenLevel);
    return {};
}
void Application::unload_level(sim::RunEndReason reason, bool show_results) {
    auto* const level{storage_.get()};
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
        endpoint_.present_menu(menu_);
        endpoint_.finish_level({completed, score.has_value(), saved.new_best_time, show_results});
        endpoint_.publish();
    }

    storage_.unload();
}
}
