#include <ioj/sim/player/game_settings_session.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace ioj {
void GameSettingsSession::initialise(GameSettingsState applied,
                                     GameSettingsState defaults,
                                     sim::player::FlightModelLoadout loadout) {
    values_.begin(std::move(applied), std::move(defaults));
    applied_loadout_ = std::move(loadout);
    pending_loadout_ = applied_loadout_;
    phase_ = GameSettingsSessionPhase::Inactive;
}
auto GameSettingsSession::begin_edit(GameSettingsState applied, GameSettingsState defaults)
    -> bool {
    if (is_awaiting_display_confirmation()) {
        return false;
    }
    values_.begin(std::move(applied), std::move(defaults));
    pending_loadout_ = applied_loadout_;
    phase_ = GameSettingsSessionPhase::Editing;
    return true;
}
auto GameSettingsSession::cancel() -> bool {
    if (phase_ != GameSettingsSessionPhase::Editing) {
        return false;
    }
    values_.cancel();
    pending_loadout_ = applied_loadout_;
    phase_ = GameSettingsSessionPhase::Inactive;
    return true;
}
auto GameSettingsSession::apply(double const now, bool const external_changes)
    -> GameSettingsApplyResult {
    assert(std::isfinite(now));
    if (phase_ != GameSettingsSessionPhase::Editing || (!is_dirty() && !external_changes)) {
        return GameSettingsApplyResult::Ignored;
    }

    auto const& pending{values_.pending()};
    auto const& applied{values_.applied()};
    auto const display_changed{pending.resolution != applied.resolution ||
                               pending.window_mode != applied.window_mode};
    applied_loadout_ = pending_loadout_;
    if (!display_changed) {
        values_.commit_all();
        return GameSettingsApplyResult::Applied;
    }

    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.apply_mode != SettingApplyMode::Confirm) {
            values_.commit_setting(descriptor.id);
        }
    }
    phase_ = GameSettingsSessionPhase::ConfirmingDisplay;
    confirmation_deadline_ = now + 15.0;
    return GameSettingsApplyResult::ConfirmDisplay;
}
auto GameSettingsSession::confirm_display() -> bool {
    if (!is_awaiting_display_confirmation()) {
        return false;
    }
    values_.commit_all();
    phase_ = GameSettingsSessionPhase::Editing;
    return true;
}
auto GameSettingsSession::revert_display() -> bool {
    if (!is_awaiting_display_confirmation()) {
        return false;
    }
    values_.cancel();
    phase_ = GameSettingsSessionPhase::Editing;
    return true;
}
auto GameSettingsSession::display_confirmation_expired(double const now) const -> bool {
    return is_awaiting_display_confirmation() && now >= confirmation_deadline_;
}
auto GameSettingsSession::display_confirmation_seconds_remaining(double const now) const
    -> std::int32_t {
    return is_awaiting_display_confirmation()
             ? static_cast<std::int32_t>(
                   std::clamp(std::ceil(confirmation_deadline_ - now), 0.0, 15.0))
             : 0;
}
auto GameSettingsSession::set_setting(GameSetting const setting, GameSettingValue const& value)
    -> bool {
    return phase_ == GameSettingsSessionPhase::Editing && values_.set_setting(setting, value);
}
auto GameSettingsSession::reset_category(GameSettingCategory const category) -> bool {
    if (phase_ != GameSettingsSessionPhase::Editing) {
        return false;
    }
    values_.reset_category(category);
    if (category == GameSettingCategory::Controls) {
        pending_loadout_ = sim::player::make_default_flight_model_loadout();
    }
    return true;
}
auto GameSettingsSession::is_dirty() const -> bool {
    return values_.is_dirty() || pending_loadout_ != applied_loadout_;
}
auto GameSettingsSession::is_dirty(GameSettingCategory const category) const -> bool {
    return values_.is_dirty(category) ||
           (category == GameSettingCategory::Controls && pending_loadout_ != applied_loadout_);
}
auto GameSettingsSession::is_at_defaults(GameSettingCategory const category) const -> bool {
    return values_.is_at_defaults(category) &&
           (category != GameSettingCategory::Controls ||
            pending_loadout_ == sim::player::make_default_flight_model_loadout());
}
auto GameSettingsSession::flight_model_profile(sim::player::FlightModelSlot const slot) const
    -> sim::player::FlightModelProfile const& {
    return sim::player::flight_model_profile(pending_loadout_, slot);
}
auto GameSettingsSession::set_flight_model_profile(sim::player::FlightModelSlot const slot,
                                                   sim::player::FlightModelProfile profile)
    -> bool {
    if (phase_ != GameSettingsSessionPhase::Editing) {
        return false;
    }
    auto& target{sim::player::flight_model_profile(pending_loadout_, slot)};
    auto const preset{target.base_preset};
    if (profile.base_preset != preset ||
        !sim::player::matches_authored_flight_model_topology(profile.config, preset)) {
        return false;
    }
    profile.customized = true;
    return sim::player::try_set_flight_model_profile(pending_loadout_, slot, profile);
}
}
