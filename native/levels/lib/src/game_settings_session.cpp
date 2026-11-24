#include <ioj/settings/game_settings_session.h>

#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_value.h>
#include <ioj/settings/game_settings.h>
#include <ioj/settings/game_settings_apply_result.h>
#include <ioj/settings/game_settings_session_phase.h>
#include <ioj/settings/game_settings_state.h>
#include <ioj/settings/setting_apply_mode.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

namespace ioj {
void GameSettingsSession::initialise(GameSettingsState applied, GameSettingsState defaults) {
    values_.begin(applied, defaults);
    phase_ = GameSettingsSessionPhase::Inactive;
}
auto GameSettingsSession::begin_edit(GameSettingsState applied, GameSettingsState defaults)
    -> bool {
    if (is_awaiting_display_confirmation()) {
        return false;
    }
    values_.begin(applied, defaults);
    phase_ = GameSettingsSessionPhase::Editing;
    return true;
}
auto GameSettingsSession::cancel() -> bool {
    if (phase_ != GameSettingsSessionPhase::Editing) {
        return false;
    }
    values_.cancel();
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
    return true;
}
auto GameSettingsSession::is_dirty() const -> bool {
    return values_.is_dirty();
}
auto GameSettingsSession::is_dirty(GameSettingCategory const category) const -> bool {
    return values_.is_dirty(category);
}
auto GameSettingsSession::is_at_defaults(GameSettingCategory const category) const -> bool {
    return values_.is_at_defaults(category);
}
}
