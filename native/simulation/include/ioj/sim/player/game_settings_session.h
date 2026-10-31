#pragma once

#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/sim/player/flight_model_config.h>

namespace ioj {
enum class GameSettingsApplyResult { Ignored, Applied, ConfirmDisplay };

class GameSettingsSession {
  public:
    void initialise(GameSettingsState applied,
                    GameSettingsState defaults,
                    sim::player::FlightModelLoadout loadout);
    auto begin_edit(GameSettingsState applied, GameSettingsState defaults) -> bool;
    auto cancel() -> bool;
    auto apply(double now, bool external_changes = false) -> GameSettingsApplyResult;
    auto confirm_display() -> bool;
    auto revert_display() -> bool;
    auto display_confirmation_expired(double now) const -> bool;
    auto display_confirmation_seconds_remaining(double now) const -> std::int32_t;
    auto is_editing() const -> bool { return editing_; }
    auto is_awaiting_display_confirmation() const -> bool { return awaiting_confirmation_; }

    auto settings_state() const -> GameSettingsState const& { return values_.pending(); }
    auto applied_state() const -> GameSettingsState const& { return values_.applied(); }
    auto default_state() const -> GameSettingsState const& { return values_.defaults(); }
    auto value(GameSetting setting) const -> GameSettingValue { return values_.value(setting); }
    auto set_setting(GameSetting setting, GameSettingValue const& value) -> bool;
    auto reset_category(GameSettingCategory category) -> bool;
    auto is_dirty() const -> bool;
    auto is_dirty(GameSettingCategory category) const -> bool;
    auto is_at_defaults(GameSettingCategory category) const -> bool;
    auto flight_model_loadout() const -> sim::player::FlightModelLoadout const& {
        return pending_loadout_;
    }
    auto flight_model_profile(sim::player::FlightModelSlot slot) const
        -> sim::player::FlightModelProfile const&;
    auto set_flight_model_profile(sim::player::FlightModelSlot slot,
                                  sim::player::FlightModelProfile profile) -> bool;

  private:
    GameSettingsEditState values_;
    sim::player::FlightModelLoadout applied_loadout_{};
    sim::player::FlightModelLoadout pending_loadout_{};
    double confirmation_deadline_{};
    bool editing_{};
    bool awaiting_confirmation_{};
};
}
