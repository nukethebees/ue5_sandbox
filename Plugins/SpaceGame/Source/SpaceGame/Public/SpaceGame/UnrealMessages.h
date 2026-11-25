#pragma once

#include <ioj/game_capabilities.h>
#include <ioj/host/unreal_messages.h>
#include <ioj/profiles/profile_manager.h>
#include <ioj/profiles/save_browser.h>
#include <ioj/settings/player_settings.h>

#include <string_view>

namespace ioj {
class UOptionsDraft;
class USaveGameViewerWidget;

SPACEGAME_API auto player_settings() -> PlayerSettings const&;
SPACEGAME_API auto player_capabilities() -> GameCapabilities const&;
SPACEGAME_API auto player_profiles() -> profiles::ProfileManager const&;
auto profile_browser() -> profiles::SaveGameBrowser const&;
void queue_profile_action(host::ProfileActionKind kind,
                          std::string_view profile_name_or_id,
                          USaveGameViewerWidget* widget);
void queue_preferences(host::PreferenceEdit edit, UOptionsDraft& draft);
SPACEGAME_API void request_mission(session::PendingLevel mission);
bool request_menu(session::LevelSelectRequest request);
SPACEGAME_API auto menu_selection() -> std::optional<session::LevelSelectRequest>&;
auto supported_resolutions() -> std::span<ScreenResolution const>;
void request_simulation_mode(levels::LaunchMode mode);
auto ship_input() -> sim::player::PlayerInputEvents&;
void reset_level_input();
void clear_consumed_messages();
void detach_player();
}
