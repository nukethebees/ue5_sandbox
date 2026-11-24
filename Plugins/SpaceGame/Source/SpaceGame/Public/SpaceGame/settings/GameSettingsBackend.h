#pragma once

#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_settings_state.h>
#include <ioj/settings/launch_display.h>
#include <ioj/settings/player_settings.h>
#include <ioj/settings/screen_resolution.h>

#include <expected>
#include <string>
#include <vector>

namespace ioj {

class USpaceGameUserSettings;

class SPACEGAME_API FGameSettingsBackend final : public SettingsBackend {
  public:
    auto inspect_display() const -> std::expected<HostDisplay, std::string> override;
    auto verify_display() const -> SettingsResult override;
    void apply_non_display(GameSettingsState const& state) const override;
    auto apply_display(GameSettingsState const& state) const -> SettingsResult override;
    void confirm_display() const override;
    auto revert_display() const -> SettingsResult override;

    auto supported_resolutions(GameSettingOptionProvider provider) const
        -> std::vector<ScreenResolution>;
  private:
    auto user_settings() const -> USpaceGameUserSettings*;
    void write_non_display(USpaceGameUserSettings& settings, GameSettingsState const& state) const;
};

} // namespace ioj
