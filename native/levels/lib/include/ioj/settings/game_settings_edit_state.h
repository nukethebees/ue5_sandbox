#pragma once

#include <ioj/input/logical/actions.h>
#include <ioj/settings/game_setting_choice.h>
#include <ioj/settings/game_settings.h>

#include <optional>
#include <vector>

namespace ioj {

auto graphics_preset(GameSettingsState const& state) -> GameGraphicsPreset;
auto game_audio_volume(float value) -> float;
auto game_setting_is_available(GameSettingAvailabilityProvider provider,
                               GameSettingsState const& state) -> bool;
auto normalize_game_setting_value(GameSettingDescriptor const& descriptor,
                                  GameSettingValue const& value,
                                  std::span<GameSettingValue const> choices = {},
                                  bool restrict_choices = false) -> std::optional<GameSettingValue>;

auto game_setting_choices(GameSettingOptionProvider provider,
                          std::span<ScreenResolution const> resolutions = {})
    -> std::vector<GameSettingChoice>;

enum class BindingPrompt { Closed, Manage, Capture, Unsupported, Conflict };

enum class BindingChoice { Change, Clear, Reset, Replace, Cancel };

struct BindingEdit {
    FlightMapping mapping{};
    PhysicalKey candidate{};
    BindingPrompt prompt{};
    BindingChoice selected{};
    std::vector<FlightMapping> conflicts{};
};

class GameSettingsEditState {
  public:
    void begin(GameSettingsState applied, GameSettingsState defaults);
    void cancel();
    void commit_all();
    auto set_setting(GameSetting setting, GameSettingValue const& value) -> bool;
    auto binding_conflicts(FlightMapping mapping, PhysicalKey key) const
        -> std::vector<FlightMapping>;
    auto set_binding(FlightMapping mapping, PhysicalKey key, bool replace_conflicts) -> bool;
    void manage_binding(FlightMapping mapping);
    void choose_binding_action(BindingChoice choice);
    void capture_binding(PhysicalKey key);
    void navigate_binding(MenuCommand command, bool repeat);
    auto binding_edit() const -> BindingEdit const& { return binding_edit_; }
    auto binding_action_available(BindingChoice choice) const -> bool;
    void reset_category(GameSettingCategory category);

    auto applied() const -> GameSettingsState const&;
    auto pending() const -> GameSettingsState const&;
    auto defaults() const -> GameSettingsState const&;
    auto value(GameSetting setting) const -> GameSettingValue;
    auto is_dirty() const -> bool;
    auto is_dirty(GameSettingCategory category) const -> bool;
    auto is_at_defaults(GameSettingCategory category) const -> bool;
  private:
    void propose_binding(PhysicalKey key);

    BindingEdit binding_edit_;
    GameSettingsState applied_{};
    GameSettingsState pending_{};
    GameSettingsState defaults_{};
};

} // namespace ioj
