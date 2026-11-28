#pragma once

#include <ioj/host/game_host_api.h>
#include <ioj/host/messages.h>

#include <utility>
#include <vector>

namespace ioj::host {
struct NativeEndpoint {
    auto publish() -> UnrealMessages const& {
        IojPlayer_Exchange(messages(), incoming_);
        clear();
        return incoming_;
    }
    void release_replies() {
        incoming_ = {};
        IojPlayer_ReleaseMessages();
    }
    auto size() const -> std::size_t { return outgoing_.order.size(); }
    auto messages() const -> NativeMessages {
        return outgoing_.with_columns(
            [](auto const&... columns) -> NativeMessages { return {columns...}; });
    }
    void clear() {
        outgoing_.with_columns([](auto&... columns) { (columns.clear(), ...); });
    }
    void send_key(MenuKey value) {
        outgoing_.order.push_back({NativeMessageKind::Key, outgoing_.keys.size()});
        outgoing_.keys.push_back(std::move(value));
    }
    void send_text(MenuText value) {
        outgoing_.order.push_back({NativeMessageKind::Text, outgoing_.texts.size()});
        outgoing_.texts.push_back(std::move(value));
    }
    void send_pointer(MenuPointer value) {
        outgoing_.order.push_back({NativeMessageKind::Pointer, outgoing_.pointers.size()});
        outgoing_.pointers.push_back(std::move(value));
    }
    void present_menu(MenuPresentation const& state, std::optional<MenuUpdate> update = {}) {
        outgoing_.order.push_back({NativeMessageKind::Menu, outgoing_.menus.size()});
        outgoing_.menus.emplace_back(state, std::move(update));
    }
    void present_pause(PauseCommand value) {
        outgoing_.order.push_back({NativeMessageKind::Pause, outgoing_.pause_commands.size()});
        outgoing_.pause_commands.push_back(std::move(value));
    }
    void select_benchmark(BenchmarkAction value) {
        outgoing_.order.push_back(
            {NativeMessageKind::Benchmark, outgoing_.benchmark_selections.size()});
        outgoing_.benchmark_selections.push_back(std::move(value));
    }
    void step_viewer(ViewerStep value) {
        outgoing_.order.push_back({NativeMessageKind::Viewer, outgoing_.viewer_steps.size()});
        outgoing_.viewer_steps.push_back(std::move(value));
    }
    void present_steering(std::optional<Vector2d> value) {
        outgoing_.order.push_back({NativeMessageKind::Steering, outgoing_.steering.size()});
        outgoing_.steering.push_back(std::move(value));
    }
    void present_settings(SettingsPresentation value) {
        outgoing_.order.push_back({NativeMessageKind::Settings, outgoing_.settings.size()});
        outgoing_.settings.push_back(std::move(value));
    }
    void present_catalog(presentation::LevelCatalogView value) {
        outgoing_.order.push_back({NativeMessageKind::Catalog, outgoing_.catalogs.size()});
        outgoing_.catalogs.push_back(std::move(value));
    }
    void present_profiles(presentation::ProfileView value) {
        outgoing_.order.push_back({NativeMessageKind::Profiles, outgoing_.profiles.size()});
        outgoing_.profiles.push_back(std::move(value));
    }
    void open_menu(std::string error) {
        outgoing_.order.push_back({NativeMessageKind::OpenMenu, outgoing_.errors.size()});
        outgoing_.errors.push_back(std::move(error));
    }
    void initialize_level(LevelInitialization value) {
        outgoing_.order.push_back(
            {NativeMessageKind::InitializeLevel, outgoing_.initializations.size()});
        outgoing_.initializations.push_back(std::move(value));
    }
    void present_frame(LevelPresentation value) {
        outgoing_.order.push_back({NativeMessageKind::Frame, outgoing_.frames.size()});
        outgoing_.frames.push_back(std::move(value));
    }
    void finish_level(LevelCompletion value) {
        outgoing_.order.push_back({NativeMessageKind::Finish, outgoing_.completions.size()});
        outgoing_.completions.push_back(std::move(value));
    }
    void profile_result(profiles::CreateProfileResult value) {
        outgoing_.order.push_back(
            {NativeMessageKind::ProfileResult, outgoing_.profile_results.size()});
        outgoing_.profile_results.push_back(std::move(value));
    }
    void apply_settings(ApplySettings value) {
        outgoing_.order.push_back({NativeMessageKind::ApplySettings, outgoing_.preferences.size()});
        outgoing_.preferences.push_back(std::move(value));
    }
    void show_error(std::string value) {
        outgoing_.order.push_back({NativeMessageKind::Error, outgoing_.errors.size()});
        outgoing_.errors.push_back(std::move(value));
    }
    void present_keyboard(KeyboardLayout const& value) {
        outgoing_.order.push_back({NativeMessageKind::Keyboard, outgoing_.keyboards.size()});
        outgoing_.keyboards.emplace_back(value);
    }
    void present_capabilities(SystemCapabilities const& value) {
        outgoing_.order.push_back({NativeMessageKind::Capabilities, outgoing_.capabilities.size()});
        outgoing_.capabilities.emplace_back(value);
    }
    void send(NativeMessageKind kind) { outgoing_.order.push_back({kind, 0}); }

  private:
    UnrealMessages incoming_{};
    NativeMessageColumns<std::vector> outgoing_;
};
}
