#pragma once

#include <ioj/host/mission_startup.h>
#include <ioj/settings/player_settings.h>

namespace ioj {
auto take_prepared_settings() -> PlayerSettings;
void report_mission_startup(MissionStartup status);
auto inspect_game_window() -> std::expected<WindowDisplaySettings, std::string>;
auto preview_game_window(WindowDisplaySettings display) -> SettingsResult;
auto verify_game_window() -> SettingsResult;
void keep_game_window();
auto revert_game_window() -> SettingsResult;
}
