#pragma once

#include <ioj/settings/game_settings.h>

#include <CoreMinimal.h>

class ULocalPlayer;

namespace ioj {

class USpaceGameUserSettings;
class USpaceGameInputUserSettings;

struct SPACEGAME_API FGameSettingOption {
    GameSettingValue value;
    FText label;
};

class SPACEGAME_API FGameSettingsBackend {
  public:
    void set_local_player(ULocalPlayer* local_player);
    auto read() const -> GameSettingsState;
    auto defaults() const -> GameSettingsState;

    void preview_immediate(GameSettingsState const& state, GameSetting setting) const;
    void apply_non_display(GameSettingsState const& state) const;
    void apply_display(GameSettingsState const& state) const;
    void confirm_display() const;
    void revert_display() const;
    void save() const;

    auto options(GameSettingOptionProvider provider) const -> TArray<FGameSettingOption>;
  private:
    auto user_settings() const -> USpaceGameUserSettings*;
    auto input_user_settings() const -> USpaceGameInputUserSettings*;
    void apply_input_settings(GameSettingsState const& state) const;
    void write_non_display(USpaceGameUserSettings& settings, GameSettingsState const& state) const;

    ULocalPlayer* local_player_{};
};

} // namespace ioj
