#pragma once

#include <ioj/input/dispatch/events.h>
#include <ioj/input/dispatch/player_menu.h>
#include <ioj/input/physical/keyboard_layout.h>
#include <ioj/presentation/level_catalog.h>
#include <ioj/presentation/level_frame.h>
#include <ioj/presentation/level_init.h>
#include <ioj/presentation/profile_view.h>
#include <ioj/profiles/create_profile_result.h>
#include <ioj/settings/player_settings_view.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace ioj {
struct SystemCapabilities;
namespace profiles {
struct CompletedLevel;
}
namespace host {
struct MenuMessage {
    MenuPresentation const& state;
    std::optional<MenuUpdate> update;
};
struct SettingsPresentation {
    PlayerSettingsView values;
    std::span<ScreenResolution const> resolutions;
};
struct LevelInitialization {
    presentation::LevelInit level;
    presentation::LevelFrame frame;
};
struct LevelPresentation {
    double seconds;
    presentation::LevelFrame frame;
};
struct LevelCompletion {
    profiles::CompletedLevel const& level;
    bool scored;
    bool new_best;
    bool show_results;
};
enum class PreferenceOperation { Startup, Save, Restore };
struct ApplySettings {
    PreferenceOperation operation;
    GameSettingCategory category;
    GameSettingsState values;
};
struct SettingsApplied {
    PreferenceOperation operation;
    GameSettingCategory category;
    std::string error;
};
struct SettingChange {
    GameSetting setting;
    GameSettingValue value;
};
struct MenuTextEdit {
    MenuControl control;
    std::string value;
};
struct EngineStatus {
    bool ready;
    bool world_ready;
    bool presents_level;
    bool exit_requested;
    bool failed;
    bool operator==(EngineStatus const&) const = default;
};
enum class NativeMessageKind {
    Key,
    Text,
    Pointer,
    Menu,
    Pause,
    Benchmark,
    Viewer,
    Steering,
    Settings,
    Catalog,
    Profiles,
    OpenMenu,
    InitializeLevel,
    Frame,
    Finish,
    ProfileResult,
    ApplySettings,
    Error,
    Keyboard,
    Capabilities,
    OpenLevel,
    SimulationPaused
};
struct NativeMessageHandle {
    NativeMessageKind kind;
    std::size_t index;
};
struct NativeMessages {
    std::span<NativeMessageHandle const> order;
    std::span<MenuKey const> keys;
    std::span<MenuText const> texts;
    std::span<MenuPointer const> pointers;
    std::span<MenuMessage const> menus;
    std::span<PauseCommand const> pause_commands;
    std::span<std::uint8_t const> benchmark_selections;
    std::span<ViewerStep const> viewer_steps;
    std::span<std::optional<Vector2d> const> steering;
    std::span<SettingsPresentation const> settings;
    std::span<presentation::LevelCatalogView const> catalogs;
    std::span<presentation::ProfileView const> profiles;
    std::span<LevelInitialization const> initializations;
    std::span<LevelPresentation const> frames;
    std::span<LevelCompletion const> completions;
    std::span<profiles::CreateProfileResult const> profile_results;
    std::span<ApplySettings const> preferences;
    std::span<std::string const> errors;
    std::span<KeyboardLayout const* const> keyboards;
    std::span<SystemCapabilities const* const> capabilities;
};
enum class UnrealMessageKind {
    Hit,
    PauseInteraction,
    Action,
    Setting,
    EditText,
    SimulationMode,
    Applied,
    LevelInitialized,
    PausePresented,
    Status,
    RefreshCatalog,
    RefreshProfiles,
    ResetTestProfile
};
struct UnrealMessageHandle {
    UnrealMessageKind kind;
    std::size_t index;
};
struct UnrealMessages {
    std::span<UnrealMessageHandle const> order;
    std::span<MenuTarget const> hits;
    std::span<PauseWidgetEvent const> pause_events;
    std::span<PlayerAction const> actions;
    std::span<SettingChange const> settings;
    std::span<MenuTextEdit const> edits;
    std::span<levels::LaunchMode const> simulation_modes;
    std::span<SettingsApplied const> settings_results;
    std::span<std::string const> level_results;
    std::span<std::uint8_t const> pause_results;
    std::span<EngineStatus const> status;
};
}
}
