#pragma once

#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_settings_state.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/player_settings.h>
#include <ioj/settings/screen_resolution.h>
#include <ioj/settings/video_settings.h>
#include <ioj/settings/window_display_settings.h>

#include <expected>
#include <string>
#include <vector>

namespace ioj {

class USpaceGameUserSettings;

class SPACEGAME_API FGameVideoSettings {
  public:
    auto inspect_display() const -> std::expected<WindowDisplaySettings, std::string>;
    auto verify_display() const -> SettingsResult;
    auto apply_quality(VideoSettings const& state) const -> SettingsResult;
    auto apply_display(GameSettingsState const& state) const -> SettingsResult;
    void confirm_display() const;
    auto revert_display() const -> SettingsResult;

    auto supported_resolutions(GameSettingOptionProvider provider, GameWindowMode mode) const
        -> std::vector<ScreenResolution>;
  private:
    auto user_settings() const -> USpaceGameUserSettings*;
    void write_non_display(USpaceGameUserSettings& settings, VideoSettings const& state) const;
};

} // namespace ioj
