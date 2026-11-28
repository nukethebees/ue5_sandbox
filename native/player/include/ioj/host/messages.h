#pragma once

#include <ioj/input/physical/keyboard_layout.h>
#include <ioj/input/presentation_events.h>
#include <ioj/menu/player_menu.h>
#include <ioj/presentation/level_catalog.h>
#include <ioj/presentation/level_frame.h>
#include <ioj/presentation/level_init.h>
#include <ioj/presentation/profile_view.h>
#include <ioj/profiles/create_profile_result.h>
#include <ioj/settings/player_settings_view.h>

#include <cstddef>
#include <cstdint>
#include <functional>
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
    OpenLevel
};
struct NativeMessageHandle {
    NativeMessageKind kind;
    std::size_t index;
};
template <typename T>
using MessageSpan = std::span<T const>;

template <template <typename> class Column>
struct NativeMessageColumns {
    auto with_columns(this auto& self, auto operation) {
        return operation(self.order,
                         self.keys,
                         self.texts,
                         self.pointers,
                         self.menus,
                         self.pause_commands,
                         self.benchmark_selections,
                         self.viewer_steps,
                         self.steering,
                         self.settings,
                         self.catalogs,
                         self.profiles,
                         self.initializations,
                         self.frames,
                         self.completions,
                         self.profile_results,
                         self.preferences,
                         self.errors,
                         self.keyboards,
                         self.capabilities);
    }

    Column<NativeMessageHandle> order;
    Column<MenuKey> keys;
    Column<MenuText> texts;
    Column<MenuPointer> pointers;
    Column<MenuMessage> menus;
    Column<PauseCommand> pause_commands;
    Column<BenchmarkAction> benchmark_selections;
    Column<ViewerStep> viewer_steps;
    Column<std::optional<Vector2d>> steering;
    Column<SettingsPresentation> settings;
    Column<presentation::LevelCatalogView> catalogs;
    Column<presentation::ProfileView> profiles;
    Column<LevelInitialization> initializations;
    Column<LevelPresentation> frames;
    Column<LevelCompletion> completions;
    Column<profiles::CreateProfileResult> profile_results;
    Column<ApplySettings> preferences;
    Column<std::string> errors;
    Column<std::reference_wrapper<KeyboardLayout const>> keyboards;
    Column<std::reference_wrapper<SystemCapabilities const>> capabilities;
};
using NativeMessages = NativeMessageColumns<MessageSpan>;
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
template <template <typename> class Column>
struct UnrealMessageColumns {
    auto with_columns(this auto& self, auto operation) {
        return operation(self.order,
                         self.hits,
                         self.pause_events,
                         self.actions,
                         self.settings,
                         self.edits,
                         self.simulation_modes,
                         self.settings_results,
                         self.level_results,
                         self.pause_results,
                         self.status);
    }

    Column<UnrealMessageHandle> order;
    Column<MenuTarget> hits;
    Column<PauseWidgetEvent> pause_events;
    Column<PlayerAction> actions;
    Column<SettingChange> settings;
    Column<MenuTextEdit> edits;
    Column<levels::LaunchMode> simulation_modes;
    Column<SettingsApplied> settings_results;
    Column<std::string> level_results;
    Column<std::uint8_t> pause_results;
    Column<EngineStatus> status;
};
using UnrealMessages = UnrealMessageColumns<MessageSpan>;
}
}
