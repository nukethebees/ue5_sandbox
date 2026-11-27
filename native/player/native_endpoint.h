#pragma once

#include <ioj/host/game_host_api.h>
#include <ioj/host/messages.h>

#include <utility>
#include <vector>

namespace ioj::host {
struct NativeEndpoint {
    auto exchange() -> UnrealMessages const& {
        IojPlayer_Exchange(messages(), incoming_);
        clear();
        return incoming_;
    }
    void release_replies() {
        incoming_ = {};
        IojPlayer_ReleaseMessages();
    }
    auto size() const -> std::size_t { return order.size(); }
    auto messages() const -> NativeMessages {
        return {order,
                keys,
                texts,
                pointers,
                menus,
                pause_commands,
                benchmark_selections,
                viewer_steps,
                steering,
                settings,
                catalogs,
                profiles,
                initializations,
                frames,
                completions,
                profile_results,
                preferences,
                errors,
                keyboards,
                capabilities};
    }
    void clear() {
        order.clear();
        keys.clear();
        texts.clear();
        pointers.clear();
        pause_commands.clear();
        benchmark_selections.clear();
        viewer_steps.clear();
        steering.clear();
        settings.clear();
        catalogs.clear();
        profiles.clear();
        initializations.clear();
        frames.clear();
        completions.clear();
        profile_results.clear();
        preferences.clear();
        errors.clear();
        menus.clear();
        keyboards.clear();
        capabilities.clear();
    }
    void send_key(MenuKey value) {
        order.push_back({NativeMessageKind::Key, keys.size()});
        keys.push_back(std::move(value));
    }
    void send_text(MenuText value) {
        order.push_back({NativeMessageKind::Text, texts.size()});
        texts.push_back(std::move(value));
    }
    void send_pointer(MenuPointer value) {
        order.push_back({NativeMessageKind::Pointer, pointers.size()});
        pointers.push_back(std::move(value));
    }
    void present_menu(MenuPresentation const& state, std::optional<MenuUpdate> update = {}) {
        order.push_back({NativeMessageKind::Menu, menus.size()});
        menus.emplace_back(state, std::move(update));
    }
    void present_pause(PauseCommand value) {
        order.push_back({NativeMessageKind::Pause, pause_commands.size()});
        pause_commands.push_back(std::move(value));
    }
    void select_benchmark(std::uint8_t value) {
        order.push_back({NativeMessageKind::Benchmark, benchmark_selections.size()});
        benchmark_selections.push_back(std::move(value));
    }
    void step_viewer(ViewerStep value) {
        order.push_back({NativeMessageKind::Viewer, viewer_steps.size()});
        viewer_steps.push_back(std::move(value));
    }
    void present_steering(std::optional<Vector2d> value) {
        order.push_back({NativeMessageKind::Steering, steering.size()});
        steering.push_back(std::move(value));
    }
    void present_settings(SettingsPresentation value) {
        order.push_back({NativeMessageKind::Settings, settings.size()});
        settings.push_back(std::move(value));
    }
    void present_catalog(presentation::LevelCatalogView value) {
        order.push_back({NativeMessageKind::Catalog, catalogs.size()});
        catalogs.push_back(std::move(value));
    }
    void present_profiles(presentation::ProfileView value) {
        order.push_back({NativeMessageKind::Profiles, profiles.size()});
        profiles.push_back(std::move(value));
    }
    void open_menu(std::string error) {
        order.push_back({NativeMessageKind::OpenMenu, errors.size()});
        errors.push_back(std::move(error));
    }
    void initialize_level(LevelInitialization value) {
        order.push_back({NativeMessageKind::InitializeLevel, initializations.size()});
        initializations.push_back(std::move(value));
    }
    void present_frame(LevelPresentation value) {
        order.push_back({NativeMessageKind::Frame, frames.size()});
        frames.push_back(std::move(value));
    }
    void finish_level(LevelCompletion value) {
        order.push_back({NativeMessageKind::Finish, completions.size()});
        completions.push_back(std::move(value));
    }
    void profile_result(profiles::CreateProfileResult value) {
        order.push_back({NativeMessageKind::ProfileResult, profile_results.size()});
        profile_results.push_back(std::move(value));
    }
    void apply_settings(ApplySettings value) {
        order.push_back({NativeMessageKind::ApplySettings, preferences.size()});
        preferences.push_back(std::move(value));
    }
    void show_error(std::string value) {
        order.push_back({NativeMessageKind::Error, errors.size()});
        errors.push_back(std::move(value));
    }
    void present_keyboard(KeyboardLayout const* value) {
        order.push_back({NativeMessageKind::Keyboard, keyboards.size()});
        keyboards.push_back(std::move(value));
    }
    void present_capabilities(SystemCapabilities const* value) {
        order.push_back({NativeMessageKind::Capabilities, capabilities.size()});
        capabilities.push_back(std::move(value));
    }
    void send(NativeMessageKind kind) { order.push_back({kind, 0}); }

  private:
    UnrealMessages incoming_{};
    std::vector<NativeMessageHandle> order;
    std::vector<MenuKey> keys;
    std::vector<MenuText> texts;
    std::vector<MenuPointer> pointers;
    std::vector<MenuMessage> menus;
    std::vector<PauseCommand> pause_commands;
    std::vector<std::uint8_t> benchmark_selections;
    std::vector<ViewerStep> viewer_steps;
    std::vector<std::optional<Vector2d>> steering;
    std::vector<SettingsPresentation> settings;
    std::vector<presentation::LevelCatalogView> catalogs;
    std::vector<presentation::ProfileView> profiles;
    std::vector<LevelInitialization> initializations;
    std::vector<LevelPresentation> frames;
    std::vector<LevelCompletion> completions;
    std::vector<profiles::CreateProfileResult> profile_results;
    std::vector<ApplySettings> preferences;
    std::vector<std::string> errors;
    std::vector<KeyboardLayout const*> keyboards;
    std::vector<SystemCapabilities const*> capabilities;
};
}
