#pragma once

#include <ioj/host/unreal_messages.h>
#include <ioj/input/dispatch/player_menu.h>
#include <ioj/input/physical/keyboard_layout.h>
#include <ioj/presentation/profile_view.h>
#include <ioj/settings/player_settings_view.h>
#include <ioj/system_capabilities.h>

#include <string_view>

namespace ioj {
class UOptionsDraft;
class ULevelSelectWidget;
class USaveGameViewerWidget;

SPACEGAME_API auto keyboard_layout() -> KeyboardLayout const&;
SPACEGAME_API auto menu_presentation() -> MenuPresentation const&;
SPACEGAME_API void edit_menu_text(MenuControl control, std::string value);
void edit_setting(GameSetting setting, GameSettingValue value);
SPACEGAME_API auto player_menu() -> PlayerMenu&;
SPACEGAME_API auto player_settings() -> PlayerSettingsView const&;
SPACEGAME_API auto system_capabilities() -> SystemCapabilities const&;
SPACEGAME_API auto player_profiles() -> presentation::ProfileView const&;
void queue_profile_action(host::ProfileActionKind kind,
                          std::string_view profile_name_or_id,
                          USaveGameViewerWidget* widget);
void queue_preferences(host::PreferenceEdit edit, UOptionsDraft& draft);
SPACEGAME_API void request_level(host::LevelSelection level);
SPACEGAME_API void request_level_catalog(ULevelSelectWidget& widget);
bool request_menu(session::LevelSelectRequest request);
SPACEGAME_API auto menu_selection() -> std::optional<session::LevelSelectRequest>&;
auto supported_resolutions() -> std::span<ScreenResolution const>;
SPACEGAME_API void hit_menu_control(MenuTarget target);
void request_player_action(host::PlayerAction action);
void report_pause_interaction(host::PauseWidgetEvent event);
void queue_input_action(host::WidgetAction action);
void request_simulation_mode(levels::LaunchMode mode);
void clear_requested_simulation_mode();
void clear_consumed_messages();
void detach_player();
}
