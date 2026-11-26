#pragma once

#include <ioj/host/unreal_messages.h>
#include <ioj/presentation/profile_view.h>
#include <ioj/settings/player_settings_view.h>
#include <ioj/sim/player/player_events.h>
#include <ioj/system_capabilities.h>

#include <string_view>

namespace ioj {
class UOptionsDraft;
class ULevelSelectWidget;
class USaveGameViewerWidget;

SPACEGAME_API auto player_settings() -> PlayerSettingsView const&;
SPACEGAME_API auto system_capabilities() -> SystemCapabilities const&;
SPACEGAME_API auto player_profiles() -> presentation::ProfileView const&;
void queue_profile_action(host::ProfileActionKind kind,
                          std::string_view profile_name_or_id,
                          USaveGameViewerWidget* widget);
void queue_preferences(host::PreferenceEdit edit, UOptionsDraft& draft);
SPACEGAME_API void request_mission(host::MissionSelection mission);
SPACEGAME_API void request_mission_catalog(ULevelSelectWidget& widget);
bool request_menu(session::LevelSelectRequest request);
SPACEGAME_API auto menu_selection() -> std::optional<session::LevelSelectRequest>&;
auto supported_resolutions() -> std::span<ScreenResolution const>;
void request_simulation_mode(levels::LaunchMode mode);
auto ship_input() -> sim::player::PlayerInputEvents&;
void reset_level_input();
void clear_consumed_messages();
void detach_player();
}
