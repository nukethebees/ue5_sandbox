#pragma once

#include <ioj/host/messages.h>

#include <utility>
#include <vector>

namespace ioj::host {
struct UnrealEndpoint {
    void receive(NativeMessages const& messages);
    auto messages() const -> UnrealMessages {
        return {order,
                hits,
                pause_events,
                actions,
                settings,
                edits,
                simulation_modes,
                settings_results,
                level_results,
                pause_results,
                status};
    }
    void clear() {
        order.clear();
        hits.clear();
        pause_events.clear();
        actions.clear();
        settings.clear();
        edits.clear();
        simulation_modes.clear();
        settings_results.clear();
        level_results.clear();
        pause_results.clear();
        status.clear();
    }
    void hit(MenuTarget value) {
        order.push_back({UnrealMessageKind::Hit, hits.size()});
        hits.push_back(std::move(value));
    }
    void pause_interaction(PauseWidgetEvent value) {
        order.push_back({UnrealMessageKind::PauseInteraction, pause_events.size()});
        pause_events.push_back(std::move(value));
    }
    void player_action(PlayerAction value) {
        order.push_back({UnrealMessageKind::Action, actions.size()});
        actions.push_back(std::move(value));
    }
    void edit_setting(SettingChange value) {
        order.push_back({UnrealMessageKind::Setting, settings.size()});
        settings.push_back(std::move(value));
    }
    void edit_text(MenuTextEdit value) {
        order.push_back({UnrealMessageKind::EditText, edits.size()});
        edits.push_back(std::move(value));
    }
    void request_simulation_mode(levels::LaunchMode value) {
        order.push_back({UnrealMessageKind::SimulationMode, simulation_modes.size()});
        simulation_modes.push_back(std::move(value));
    }
    void settings_applied(SettingsApplied value) {
        order.push_back({UnrealMessageKind::Applied, settings_results.size()});
        settings_results.push_back(std::move(value));
    }
    void level_initialized(std::string value) {
        order.push_back({UnrealMessageKind::LevelInitialized, level_results.size()});
        level_results.push_back(std::move(value));
    }
    void pause_presented(std::uint8_t value) {
        order.push_back({UnrealMessageKind::PausePresented, pause_results.size()});
        pause_results.push_back(std::move(value));
    }
    void engine_status(EngineStatus value) {
        order.push_back({UnrealMessageKind::Status, status.size()});
        status.push_back(std::move(value));
    }
    void send(UnrealMessageKind kind) { order.push_back({kind, 0}); }

  private:
    std::vector<UnrealMessageHandle> order;
    std::vector<MenuTarget> hits;
    std::vector<PauseWidgetEvent> pause_events;
    std::vector<PlayerAction> actions;
    std::vector<SettingChange> settings;
    std::vector<MenuTextEdit> edits;
    std::vector<levels::LaunchMode> simulation_modes;
    std::vector<SettingsApplied> settings_results;
    std::vector<std::string> level_results;
    std::vector<std::uint8_t> pause_results;
    std::vector<EngineStatus> status;
};
}
