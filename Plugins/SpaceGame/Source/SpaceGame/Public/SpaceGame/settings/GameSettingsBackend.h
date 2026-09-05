#pragma once

#include "SpaceGame/settings/GameSettings.generated.h"

namespace ml::ioj {

class USpaceGameUserSettings;

struct SPACEGAME_API FGameSettingOption {
    FGameSettingValue value;
    FText label;
};

class SPACEGAME_API FGameSettingsBackend {
  public:
    auto read() const -> FGameSettingsState;
    auto defaults() const -> FGameSettingsState;

    void preview_immediate(FGameSettingsState const& state, EGameSetting setting) const;
    void apply_non_display(FGameSettingsState const& state) const;
    void apply_display(FGameSettingsState const& state) const;
    void confirm_display() const;
    void revert_display() const;
    void save() const;

    auto options(EGameSettingOptionProvider provider) const -> TArray<FGameSettingOption>;
    auto is_available(EGameSettingAvailabilityProvider provider,
                      FGameSettingsState const& state) const -> bool;
  private:
    auto user_settings() const -> USpaceGameUserSettings*;
    void write_non_display(USpaceGameUserSettings& settings, FGameSettingsState const& state) const;
};

} // namespace ml::ioj
