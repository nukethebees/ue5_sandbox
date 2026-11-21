#pragma once

#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_settings_state.h>
#include <SpaceGame/settings/GameSettingOption.h>

#include <CoreMinimal.h>

class ULocalPlayer;

namespace ioj {

class USpaceGameUserSettings;
class USpaceGameInputUserSettings;

class SPACEGAME_API FGameSettingsBackend {
  public:
    void apply_non_display(GameSettingsState const& state) const;
    void apply_display(GameSettingsState const& state) const;
    void confirm_display() const;
    void revert_display() const;

    auto options(GameSettingOptionProvider provider) const -> TArray<FGameSettingOption>;
  private:
    auto user_settings() const -> USpaceGameUserSettings*;
    void write_non_display(USpaceGameUserSettings& settings, GameSettingsState const& state) const;
};

} // namespace ioj
