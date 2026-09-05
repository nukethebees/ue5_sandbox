#include "SpaceGame/settings/GameSettingsEditState.h"

namespace ml::ioj {
namespace {
auto setting_values_equal(FGameSettingValue const& left, FGameSettingValue const& right) -> bool {
    if (auto const* const left_float{std::get_if<float>(&left)}) {
        auto const* const right_float{std::get_if<float>(&right)};
        return right_float != nullptr &&
               (*left_float == *right_float ||
                (FMath::IsNaN(*left_float) && FMath::IsNaN(*right_float)));
    }
    return left == right;
}
}

void FGameSettingsEditState::begin(FGameSettingsState applied, FGameSettingsState defaults) {
    applied_ = MoveTemp(applied);
    pending_ = applied_;
    defaults_ = MoveTemp(defaults);
}

void FGameSettingsEditState::cancel() {
    pending_ = applied_;
}

void FGameSettingsEditState::commit_all() {
    applied_ = pending_;
}

void FGameSettingsEditState::commit_setting(EGameSetting const setting) {
    auto const pending_value{game_setting_value(pending_, setting)};
    auto const set{set_game_setting_value(applied_, setting, pending_value)};
    ensureMsgf(set, TEXT("Could not commit game setting %d"), static_cast<int32>(setting));
}

auto FGameSettingsEditState::set_setting(EGameSetting const setting, FGameSettingValue const& value)
    -> bool {
    return set_game_setting_value(pending_, setting, value);
}

void FGameSettingsEditState::reset_category(EGameSettingCategory const category) {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category) {
            static_cast<void>(set_game_setting_value(
                pending_, descriptor.id, game_setting_value(defaults_, descriptor.id)));
        }
    }
}

auto FGameSettingsEditState::applied() const -> FGameSettingsState const& {
    return applied_;
}

auto FGameSettingsEditState::pending() const -> FGameSettingsState const& {
    return pending_;
}

auto FGameSettingsEditState::defaults() const -> FGameSettingsState const& {
    return defaults_;
}

auto FGameSettingsEditState::value(EGameSetting const setting) const -> FGameSettingValue {
    return game_setting_value(pending_, setting);
}

auto FGameSettingsEditState::is_dirty() const -> bool {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (!setting_values_equal(game_setting_value(pending_, descriptor.id),
                                  game_setting_value(applied_, descriptor.id))) {
            return true;
        }
    }
    return false;
}

auto FGameSettingsEditState::is_dirty(EGameSettingCategory const category) const -> bool {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category &&
            !setting_values_equal(game_setting_value(pending_, descriptor.id),
                                  game_setting_value(applied_, descriptor.id))) {
            return true;
        }
    }
    return false;
}

auto FGameSettingsEditState::is_at_defaults(EGameSettingCategory const category) const -> bool {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category &&
            !setting_values_equal(game_setting_value(pending_, descriptor.id),
                                  game_setting_value(defaults_, descriptor.id))) {
            return false;
        }
    }
    return true;
}

} // namespace ml::ioj
