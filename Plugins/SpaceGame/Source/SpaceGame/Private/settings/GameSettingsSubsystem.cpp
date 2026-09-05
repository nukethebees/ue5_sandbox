#include "SpaceGame/settings/GameSettingsSubsystem.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

namespace ml::ioj {
constexpr double display_confirmation_duration_seconds{15.0};

void UGameSettingsSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    edit_state_.begin(backend_.read(), backend_.defaults());
}

void UGameSettingsSubsystem::Deinitialize() {
    if (display_confirmation_ticker_.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(display_confirmation_ticker_);
        display_confirmation_ticker_.Reset();
    }
    Super::Deinitialize();
}

void UGameSettingsSubsystem::begin_edit() {
    if (awaiting_display_confirmation_) {
        return;
    }
    edit_state_.begin(backend_.read(), backend_.defaults());
    editing_ = true;
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::cancel() {
    if (!editing_ || awaiting_display_confirmation_) {
        return;
    }
    preview_immediate_settings(edit_state_.applied());
    edit_state_.cancel();
    editing_ = false;
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::apply() {
    if (!editing_ || awaiting_display_confirmation_ || !is_dirty()) {
        return;
    }

    auto const& pending{edit_state_.pending()};
    auto const& applied{edit_state_.applied()};
    auto const display_changed{pending.resolution != applied.resolution ||
                               pending.window_mode != applied.window_mode};
    backend_.apply_non_display(pending);
    if (!display_changed) {
        backend_.save();
        edit_state_.commit_all();
        settings_changed.Broadcast();
        return;
    }

    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.apply_mode != ESettingApplyMode::Confirm) {
            edit_state_.commit_setting(descriptor.id);
        }
    }

    backend_.apply_display(edit_state_.pending());
    awaiting_display_confirmation_ = true;
    display_confirmation_deadline_ =
        FPlatformTime::Seconds() + display_confirmation_duration_seconds;
    display_confirmation_ticker_ = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &ThisClass::tick_display_confirmation), 0.25f);
    display_confirmation_changed.Broadcast(true);
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::reset_category(EGameSettingCategory const category) {
    if (!editing_ || awaiting_display_confirmation_) {
        return;
    }
    auto const before{edit_state_.pending()};
    edit_state_.reset_category(category);
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category &&
            descriptor.apply_mode == ESettingApplyMode::Immediate &&
            game_setting_value(before, descriptor.id) != edit_state_.value(descriptor.id)) {
            backend_.preview_immediate(edit_state_.pending(), descriptor.id);
        }
    }
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::confirm_display_changes() {
    if (!awaiting_display_confirmation_) {
        return;
    }
    backend_.confirm_display();
    backend_.save();
    edit_state_.commit_all();
    awaiting_display_confirmation_ = false;
    if (display_confirmation_ticker_.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(display_confirmation_ticker_);
        display_confirmation_ticker_.Reset();
    }
    display_confirmation_changed.Broadcast(false);
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::revert_display_changes() {
    if (!awaiting_display_confirmation_) {
        return;
    }
    backend_.revert_display();
    backend_.save();
    edit_state_.cancel();
    awaiting_display_confirmation_ = false;
    if (display_confirmation_ticker_.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(display_confirmation_ticker_);
        display_confirmation_ticker_.Reset();
    }
    display_confirmation_changed.Broadcast(false);
    settings_changed.Broadcast();
}

auto UGameSettingsSubsystem::settings_state() const -> FGameSettingsState const& {
    return edit_state_.pending();
}

auto UGameSettingsSubsystem::applied_state() const -> FGameSettingsState const& {
    return edit_state_.applied();
}

auto UGameSettingsSubsystem::default_state() const -> FGameSettingsState const& {
    return edit_state_.defaults();
}

auto UGameSettingsSubsystem::value(EGameSetting const setting) const -> FGameSettingValue {
    return edit_state_.value(setting);
}

void UGameSettingsSubsystem::set_setting(EGameSetting const setting,
                                         FGameSettingValue const& value) {
    if (!editing_ || awaiting_display_confirmation_) {
        return;
    }
    auto const& descriptor{game_setting_descriptor(setting)};
    auto normalized{normalize_value(descriptor, value)};
    if (!normalized.IsSet() || !edit_state_.set_setting(setting, normalized.GetValue())) {
        UE_LOG(
            LogTemp, Warning, TEXT("Rejected invalid value for game setting %s"), descriptor.name);
        return;
    }
    if (descriptor.apply_mode == ESettingApplyMode::Immediate) {
        backend_.preview_immediate(edit_state_.pending(), setting);
    }
    settings_changed.Broadcast();
}

auto UGameSettingsSubsystem::descriptors(EGameSettingCategory const category) const
    -> TArray<FGameSettingDescriptor const*> {
    TArray<FGameSettingDescriptor const*> result;
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category) {
            result.Add(&descriptor);
        }
    }
    return result;
}

auto UGameSettingsSubsystem::options(EGameSetting const setting) const
    -> TArray<FGameSettingOption> {
    return backend_.options(game_setting_descriptor(setting).options_provider);
}

auto UGameSettingsSubsystem::is_available(EGameSetting const setting) const -> bool {
    return backend_.is_available(game_setting_descriptor(setting).availability_provider,
                                 edit_state_.pending());
}

auto UGameSettingsSubsystem::is_dirty() const -> bool {
    return edit_state_.is_dirty();
}

auto UGameSettingsSubsystem::is_dirty(EGameSettingCategory const category) const -> bool {
    return edit_state_.is_dirty(category);
}

auto UGameSettingsSubsystem::is_at_defaults(EGameSettingCategory const category) const -> bool {
    return edit_state_.is_at_defaults(category);
}

auto UGameSettingsSubsystem::is_awaiting_display_confirmation() const -> bool {
    return awaiting_display_confirmation_;
}

auto UGameSettingsSubsystem::display_confirmation_seconds_remaining() const -> int32 {
    if (!awaiting_display_confirmation_) {
        return 0;
    }
    return FMath::Max(
        0, FMath::CeilToInt32(display_confirmation_deadline_ - FPlatformTime::Seconds()));
}

auto UGameSettingsSubsystem::tick_display_confirmation(float const delta_seconds) -> bool {
    static_cast<void>(delta_seconds);
    if (!awaiting_display_confirmation_) {
        return false;
    }
    if (FPlatformTime::Seconds() >= display_confirmation_deadline_) {
        revert_display_changes();
        return false;
    }
    display_confirmation_changed.Broadcast(true);
    return true;
}

void UGameSettingsSubsystem::preview_immediate_settings(FGameSettingsState const& state) {
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.apply_mode == ESettingApplyMode::Immediate) {
            backend_.preview_immediate(state, descriptor.id);
        }
    }
}

auto UGameSettingsSubsystem::normalize_value(FGameSettingDescriptor const& descriptor,
                                             FGameSettingValue const& value) const
    -> TOptional<FGameSettingValue> {
    if (descriptor.control_kind == ESettingControlKind::FloatRange) {
        auto const* typed_value{std::get_if<float>(&value)};
        if (typed_value == nullptr) {
            return {};
        }
        auto const clamped{FMath::Clamp(*typed_value,
                                        static_cast<float>(descriptor.minimum),
                                        static_cast<float>(descriptor.maximum))};
        return FGameSettingValue{clamped};
    }
    if (descriptor.control_kind == ESettingControlKind::IntegerRange) {
        auto const* typed_value{std::get_if<int32>(&value)};
        if (typed_value == nullptr) {
            return {};
        }
        auto const clamped{FMath::Clamp(*typed_value,
                                        static_cast<int32>(descriptor.minimum),
                                        static_cast<int32>(descriptor.maximum))};
        return FGameSettingValue{clamped};
    }
    if (descriptor.control_kind == ESettingControlKind::Choice) {
        auto const choices{backend_.options(descriptor.options_provider)};
        auto const found{choices.ContainsByPredicate(
            [&value](FGameSettingOption const& option) { return option.value == value; })};
        if (!found) {
            return {};
        }
    }
    return value;
}

} // namespace ml::ioj
