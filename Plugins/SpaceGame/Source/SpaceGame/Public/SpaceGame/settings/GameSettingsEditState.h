#pragma once

#include "SpaceGame/settings/GameSettings.generated.h"

namespace ml::ioj {

class SPACEGAME_API FGameSettingsEditState {
  public:
    void begin(FGameSettingsState applied, FGameSettingsState defaults);
    void cancel();
    void commit_all();
    void commit_setting(EGameSetting setting);
    auto set_setting(EGameSetting setting, FGameSettingValue const& value) -> bool;
    void reset_category(EGameSettingCategory category);

    auto applied() const -> FGameSettingsState const&;
    auto pending() const -> FGameSettingsState const&;
    auto defaults() const -> FGameSettingsState const&;
    auto value(EGameSetting setting) const -> FGameSettingValue;
    auto is_dirty() const -> bool;
    auto is_dirty(EGameSettingCategory category) const -> bool;
    auto is_at_defaults(EGameSettingCategory category) const -> bool;
  private:
    FGameSettingsState applied_{};
    FGameSettingsState pending_{};
    FGameSettingsState defaults_{};
};

} // namespace ml::ioj
