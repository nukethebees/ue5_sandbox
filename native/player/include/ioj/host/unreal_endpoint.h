#pragma once

#include <ioj/host/messages.h>
#include <ioj/system_capabilities.h>

#include <utility>
#include <vector>

namespace ioj {
class ATestBatchOrchestrator;
}
namespace ioj::host {
struct UnrealEndpoint {
    auto menu() const -> MenuPresentation const& { return menu_; }
    auto settings() const -> PlayerSettingsView const& { return settings_; }
    auto keyboard() const -> KeyboardLayout const& { return keyboard_; }
    auto capabilities() const -> SystemCapabilities const& { return capabilities_; }
    auto resolutions() const -> std::span<ScreenResolution const> { return resolutions_; }
    auto launch_error() const -> std::string_view { return menu_error_; }
    void level_world_ready(ATestBatchOrchestrator& actor);
    void release_world(ATestBatchOrchestrator& actor);
    void receive(NativeMessages const& messages);
    auto messages() const -> UnrealMessages {
        return outgoing_.with_columns(
            [](auto const&... columns) -> UnrealMessages { return {columns...}; });
    }
    void clear() {
        outgoing_.with_columns([](auto&... columns) { (columns.clear(), ...); });
    }
    void hit(MenuTarget value) {
        outgoing_.order.push_back({UnrealMessageKind::Hit, outgoing_.hits.size()});
        outgoing_.hits.push_back(std::move(value));
    }
    void pause_interaction(PauseWidgetEvent value) {
        outgoing_.order.push_back(
            {UnrealMessageKind::PauseInteraction, outgoing_.pause_events.size()});
        outgoing_.pause_events.push_back(std::move(value));
    }
    void player_action(PlayerAction value) {
        outgoing_.order.push_back({UnrealMessageKind::Action, outgoing_.actions.size()});
        outgoing_.actions.push_back(std::move(value));
    }
    void edit_setting(SettingChange value) {
        outgoing_.order.push_back({UnrealMessageKind::Setting, outgoing_.settings.size()});
        outgoing_.settings.push_back(std::move(value));
    }
    void edit_text(MenuTextEdit value) {
        outgoing_.order.push_back({UnrealMessageKind::EditText, outgoing_.edits.size()});
        outgoing_.edits.push_back(std::move(value));
    }
    void request_simulation_mode(levels::LaunchMode value) {
        outgoing_.order.push_back(
            {UnrealMessageKind::SimulationMode, outgoing_.simulation_modes.size()});
        outgoing_.simulation_modes.push_back(std::move(value));
    }
    void settings_applied(SettingsApplied value) {
        outgoing_.order.push_back({UnrealMessageKind::Applied, outgoing_.settings_results.size()});
        outgoing_.settings_results.push_back(std::move(value));
    }
    void level_initialized(std::string value) {
        outgoing_.order.push_back(
            {UnrealMessageKind::LevelInitialized, outgoing_.level_results.size()});
        outgoing_.level_results.push_back(std::move(value));
    }
    void pause_presented(std::uint8_t value) {
        outgoing_.order.push_back({UnrealMessageKind::PausePresented, outgoing_.pause_results.size()});
        outgoing_.pause_results.push_back(std::move(value));
    }
    void send(UnrealMessageKind kind) { outgoing_.order.push_back({kind, 0}); }

  private:
    UnrealMessageColumns<std::vector> outgoing_;
    MenuPresentation menu_;
    PlayerSettingsView settings_;
    KeyboardLayout keyboard_;
    SystemCapabilities capabilities_;
    std::vector<ScreenResolution> resolutions_;
    std::string menu_error_;
    ATestBatchOrchestrator* level_world_{};
};
}
