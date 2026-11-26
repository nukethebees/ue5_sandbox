#pragma once

#include <ioj/settings/application_settings.h>
#include <ioj/settings/game_setting_choice.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/settings/launch_display_mode.h>

#include <span>
#include <vector>

namespace ioj {
struct PlayerSettingsView {
    auto is_available(GameSettingsState const& draft, GameSetting setting) const -> bool;
    auto choices(GameSettingsState const& draft,
                 GameSettingOptionProvider provider,
                 std::span<ScreenResolution const> resolutions) const
        -> std::vector<GameSettingChoice>;
    auto set_setting(GameSettingsEditState& edit,
                     GameSetting setting,
                     GameSettingValue const& value,
                     std::span<ScreenResolution const> resolutions) const -> bool;

    ApplicationSettings application;
    GameSettingsState applied_values;
    GameSettingsState defaults;
    LaunchDisplayMode launch_mode;
    ScreenResolution saved_resolution;
};
}
