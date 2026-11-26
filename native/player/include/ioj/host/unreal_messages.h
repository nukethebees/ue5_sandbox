#pragma once

#include <ioj/session/level_select_request.h>
#include <ioj/session/mission_launch_request.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_settings_state.h>
#include <ioj/sim/player/player_events.h>

#include <optional>
#include <span>
#include <string>
#include <variant>

namespace ioj::host {
enum class ProfileActionKind { Create, Activate, Refresh, SelectReport, ResetTestProfile };

struct ProfileAction {
    ProfileActionKind kind;
    std::string profile_name_or_id;
};

enum class PreferenceAction { Save, PreviewDisplay, RevertDisplay };

struct PreferenceEdit {
    PreferenceAction action;
    GameSettingCategory category;
    GameSettingsState values;
    GameSettingsState original_settings;
};

using NavigationRequest =
    std::variant<std::monostate, session::MissionLaunchRequest, session::LevelSelectRequest>;

struct UnrealMessages {
    std::span<ProfileAction const> profile_actions;
    std::span<PreferenceEdit const> preference_edits;
    NavigationRequest const& navigation_request;
    std::optional<levels::LaunchMode> requested_simulation_mode;
    sim::player::PlayerInputEvents const& ship_input;
};
}
