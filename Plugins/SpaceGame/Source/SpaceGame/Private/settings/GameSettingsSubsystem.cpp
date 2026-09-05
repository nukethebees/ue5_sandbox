#include "SpaceGame/settings/GameSettingsSubsystem.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

namespace ml::ioj {
namespace {

constexpr double display_confirmation_duration_seconds{15.0};

} // namespace

void UGameSettingsSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    baseline_ = backend_.read();
    pending_ = baseline_;
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
    baseline_ = backend_.read();
    pending_ = baseline_;
    editing_ = true;
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::cancel() {
    if (!editing_ || awaiting_display_confirmation_) {
        return;
    }
    preview_immediate_settings(baseline_);
    pending_ = baseline_;
    editing_ = false;
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::apply() {
    if (!editing_ || awaiting_display_confirmation_ || !is_dirty()) {
        return;
    }

    auto const display_changed{pending_.resolution != baseline_.resolution ||
                               pending_.window_mode != baseline_.window_mode};
    backend_.apply_non_display(pending_);
    if (!display_changed) {
        backend_.save();
        baseline_ = pending_;
        settings_changed.Broadcast();
        return;
    }

    auto applied_non_display{pending_};
    applied_non_display.resolution = baseline_.resolution;
    applied_non_display.window_mode = baseline_.window_mode;
    baseline_ = applied_non_display;

    backend_.apply_display(pending_);
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
    auto const defaults{backend_.defaults()};
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category != category) {
            continue;
        }
        auto const default_value{game_setting_value(defaults, descriptor.id)};
        set_setting(descriptor.id, default_value);
    }
}

void UGameSettingsSubsystem::confirm_display_changes() {
    if (!awaiting_display_confirmation_) {
        return;
    }
    backend_.confirm_display();
    backend_.save();
    baseline_ = pending_;
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
    pending_ = baseline_;
    awaiting_display_confirmation_ = false;
    if (display_confirmation_ticker_.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(display_confirmation_ticker_);
        display_confirmation_ticker_.Reset();
    }
    display_confirmation_changed.Broadcast(false);
    settings_changed.Broadcast();
}

auto UGameSettingsSubsystem::settings_state() const -> FGameSettingsState const& {
    return pending_;
}

auto UGameSettingsSubsystem::value(EGameSetting const setting) const -> FGameSettingValue {
    return game_setting_value(pending_, setting);
}

void UGameSettingsSubsystem::set_setting(EGameSetting const setting,
                                         FGameSettingValue const& value) {
    if (!editing_ || awaiting_display_confirmation_) {
        return;
    }
    auto const& descriptor{game_setting_descriptor(setting)};
    auto normalized{normalize_value(descriptor, value)};
    if (!normalized.IsSet() || !set_game_setting_value(pending_, setting, normalized.GetValue())) {
        UE_LOG(
            LogTemp, Warning, TEXT("Rejected invalid value for game setting %s"), descriptor.name);
        return;
    }
    if (descriptor.apply_mode == ESettingApplyMode::Immediate) {
        backend_.preview_immediate(pending_, setting);
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
    return backend_.is_available(game_setting_descriptor(setting).availability_provider, pending_);
}

auto UGameSettingsSubsystem::is_dirty() const -> bool {
    return pending_ != baseline_;
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
