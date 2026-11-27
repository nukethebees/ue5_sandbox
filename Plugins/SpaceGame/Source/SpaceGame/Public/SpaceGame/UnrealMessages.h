#pragma once

#include <ioj/host/messages.h>
#include <ioj/system_capabilities.h>

namespace ioj {
SPACEGAME_API auto menu_presentation() -> MenuPresentation const&;
SPACEGAME_API auto player_settings() -> PlayerSettingsView const&;
SPACEGAME_API auto system_capabilities() -> SystemCapabilities const&;
SPACEGAME_API auto keyboard_layout() -> KeyboardLayout const&;
SPACEGAME_API auto supported_resolutions() -> std::span<ScreenResolution const>;
SPACEGAME_API void hit_menu_control(MenuTarget target);
SPACEGAME_API void edit_menu_text(MenuControl control, std::string value);
void edit_setting(GameSetting setting, GameSettingValue value);
void report_pause_interaction(host::PauseWidgetEvent event);
void request_player_action(host::PlayerAction action);
SPACEGAME_API void request_level_catalog();
void request_profile_refresh();
void request_simulation_mode(levels::LaunchMode mode);
SPACEGAME_API auto launch_error() -> std::string_view;
}
