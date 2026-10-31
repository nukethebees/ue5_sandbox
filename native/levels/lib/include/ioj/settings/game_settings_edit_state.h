#pragma once

#include <ioj/settings/game_settings.h>

#include <optional>
#include <string>
#include <vector>

namespace ioj {

auto graphics_preset(GameSettingsState const& state) -> GameGraphicsPreset;
auto game_anti_aliasing_method(std::int32_t stored_value) -> GameAntiAliasingMethod;
auto game_audio_volume(float value) -> float;
auto game_bees_count(std::int32_t value) -> std::int32_t;
auto game_setting_is_available(GameSettingAvailabilityProvider provider,
                               GameSettingsState const& state) -> bool;
auto normalize_game_setting_value(GameSettingDescriptor const& descriptor,
                                  GameSettingValue const& value,
                                  std::span<GameSettingValue const> choices = {},
                                  bool restrict_choices = false) -> std::optional<GameSettingValue>;

struct GameSettingChoice {
    GameSettingValue value;
    std::string label;
};

auto game_setting_choices(GameSettingOptionProvider provider,
                          std::span<ScreenResolution const> resolutions = {})
    -> std::vector<GameSettingChoice>;

class GameSettingsEditState {
  public:
    void begin(GameSettingsState applied, GameSettingsState defaults);
    void cancel();
    void commit_all();
    void commit_setting(GameSetting setting);
    auto set_setting(GameSetting setting, GameSettingValue const& value) -> bool;
    void reset_category(GameSettingCategory category);

    auto applied() const -> GameSettingsState const&;
    auto pending() const -> GameSettingsState const&;
    auto defaults() const -> GameSettingsState const&;
    auto value(GameSetting setting) const -> GameSettingValue;
    auto is_dirty() const -> bool;
    auto is_dirty(GameSettingCategory category) const -> bool;
    auto is_at_defaults(GameSettingCategory category) const -> bool;
  private:
    GameSettingsState applied_{};
    GameSettingsState pending_{};
    GameSettingsState defaults_{};
};

} // namespace ioj
