#include "SpaceGame/settings/OptionsDraft.h"

#include "input/BindingKey.h"
#include "SpaceGame/settings/ControlBindingAddress.h"
#include "SpaceGame/settings/ControlBindings.h"
#include "SpaceGame/settings/ControlBindingView.h"
#include "SpaceGame/settings/ControlChordBindingView.h"
#include "SpaceGame/settings/GameSettingOption.h"
#include "SpaceGame/UnrealMessages.h"
#include <ioj/input/flight_controls.h>
#include <ioj/settings/control_bindings.h>
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_setting_value.h>
#include <ioj/settings/game_settings.h>
#include <ioj/settings/player_settings_view.h>
#include <SpaceGamePresentation/audio/GameAudio.h>

#include <SandboxCoreEngine/strings.h>

#include "Containers/Array.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/InputSettings.h"
#include "HAL/Platform.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Optional.h"
#include "Templates/Casts.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Object.h"
#include <Engine/GameInstance.h>
#include <Misc/App.h>
#include <Misc/MessageDialog.h>

#include <string>
#include <variant>

namespace ioj {
namespace {
void report_settings_error(std::string const& error) {
    auto const message{FText::FromString(to_fstring(error))};
    UE_LOG(LogTemp, Error, TEXT("%s"), *message.ToString());
    if (!FApp::IsUnattended() && !GIsAutomationTesting) {
        FMessageDialog::Open(EAppMsgType::Ok, message);
    }
}
}

/* **************************************** */
// Lifecycle
/* **************************************** */
void UOptionsDraft::load(ULocalPlayer* player, GameSettingCategory category) {
    if (save_pending_) {
        return;
    }

    check(!is_awaiting_display_confirmation());
    auto const& settings{player_settings()};
    editing_local_player_ = player;
    draft_.begin(settings.applied_values, settings.defaults);
    bindings_ = category == GameSettingCategory::Controls ? all_control_bindings()
                                                          : TArray<FControlBindingView>{};
    original_bindings_ = bindings_;
    settings_changed.Broadcast();
}

void UOptionsDraft::cancel() {
    if (save_pending_) {
        return;
    }

    if (is_awaiting_display_confirmation()) {
        revert_display_changes();
    }

    draft_.cancel();
    bindings_ = original_bindings_;
    settings_changed.Broadcast();
}

auto UOptionsDraft::apply(GameSettingCategory category) -> bool {
    if (save_pending_ || !editing_local_player_.IsValid() || is_awaiting_display_confirmation() ||
        !is_dirty()) {
        return false;
    }

    auto const& pending{draft_.pending()};
    auto const& applied{draft_.applied()};
    if (category == GameSettingCategory::Video &&
        (pending.video.window_mode != applied.video.window_mode ||
         pending.video.resolution != applied.video.resolution)) {
        save_pending_ = true;
        queue_preferences({host::PreferenceAction::PreviewDisplay, category, pending, applied},
                          *this);
        return true;
    }

    return save(category);
}

auto UOptionsDraft::save(GameSettingCategory category) -> bool {
    if (auto result{apply_settings(category, draft_.pending())}; !result) {
        report_settings_error("Applying settings failed: " + result.error());
        return false;
    }

    auto requested{draft_.pending()};
    if (category == GameSettingCategory::Controls && input_is_dirty()) {
        std::vector<KeyBinding> overrides;
        for (auto const& row : bindings_) {
            if (row.is_modified()) {
                auto const key{native_binding_key(row.current_key)};
                overrides.push_back({row.address.mapping, key});
            }
        }
        requested.controls.bindings = std::move(overrides);
    }

    queue_preferences(
        {host::PreferenceAction::Save, category, std::move(requested), draft_.applied()}, *this);
    save_pending_ = true;
    return true;
}

void UOptionsDraft::preference_finished(host::PreferenceEdit const& edit, std::string_view error) {
    check(save_pending_);
    save_pending_ = false;
    auto const category{edit.category};
    if (edit.action != host::PreferenceAction::Save) {
        if (!error.empty()) {
            report_settings_error(std::string{error});
            save_completed.Broadcast(false);
            return;
        }

        if (edit.action == host::PreferenceAction::RevertDisplay) {
            draft_.cancel();
            finish_display_confirmation();
            save_completed.Broadcast(false);
        } else {
            confirmation_deadline_ = FPlatformTime::Seconds() + 15.0;
            display_confirmation_ticker_ = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateUObject(this, &ThisClass::tick_display_confirmation), 0.25f);
            display_confirmation_changed.Broadcast(true);
        }
        return;
    }

    bool const bindings_changed{category == GameSettingCategory::Controls && input_is_dirty()};
    if (!error.empty()) {
        std::string message{error};
        auto restored{apply_settings(category, draft_.applied())};

        report_settings_error("Saving settings failed: " + message +
                              (restored ? "" : "; restoring settings failed: " + restored.error()));
        if (is_awaiting_display_confirmation()) {
            finish_display_confirmation();
        }

        save_completed.Broadcast(false);
        return;
    }

    if (bindings_changed) {
        original_bindings_ = bindings_;
    }

    draft_.begin(player_settings().applied_values, player_settings().defaults);
    settings_changed.Broadcast();
    if (is_awaiting_display_confirmation()) {
        finish_display_confirmation();
    }

    save_completed.Broadcast(true);
}

auto UOptionsDraft::apply_settings(GameSettingCategory category, GameSettingsState const& values)
    -> SettingsResult {
    if (category == GameSettingCategory::Video) {
        if (auto result{video_.apply_quality(values.video)}; !result) {
            return result;
        }
    }

    if (category == GameSettingCategory::Audio) {
        auto* audio{editing_local_player_->GetGameInstance()->GetSubsystem<UGameAudio>()};
        audio->set_master_volume(values.audio.master_volume);
        audio->set_music_volume(values.audio.music_volume);
        audio->set_sfx_volume(values.audio.sfx_volume);
        audio->set_ui_volume(values.audio.ui_volume);
    }

    return {};
}
void UOptionsDraft::reset_category(GameSettingCategory category) {
    if (!editing_local_player_.IsValid() || is_awaiting_display_confirmation()) {
        return;
    }

    draft_.reset_category(category);
    if (category == GameSettingCategory::Video) {
        if (player_settings().launch_mode != LaunchDisplayMode::Preferences) {
            draft_.set_setting(GameSetting::WindowMode, draft_.applied().video.window_mode);
        } else if (draft_.pending().video.window_mode == GameWindowMode::Fullscreen) {
            draft_.set_setting(GameSetting::WindowMode, GameWindowMode::Borderless);
        }

        if (!is_available(GameSetting::Resolution)) {
            draft_.set_setting(GameSetting::Resolution, draft_.applied().video.resolution);
        }
    } else if (category == GameSettingCategory::Controls) {
        reset_all_control_bindings();
    }

    settings_changed.Broadcast();
}

void UOptionsDraft::confirm_display_changes() {
    if (save_pending_ || !is_awaiting_display_confirmation()) {
        return;
    }

    if (!save(GameSettingCategory::Video)) {
        revert_display_changes();
    }
}

void UOptionsDraft::revert_display_changes() {
    if (save_pending_ || !is_awaiting_display_confirmation()) {
        return;
    }

    save_pending_ = true;
    queue_preferences({host::PreferenceAction::RevertDisplay,
                       GameSettingCategory::Video,
                       draft_.pending(),
                       draft_.applied()},
                      *this);
}

auto UOptionsDraft::value(GameSetting const setting) const -> GameSettingValue {
    return draft_.value(setting);
}

void UOptionsDraft::set_setting(GameSetting const setting, GameSettingValue const& value) {
    if (!editing_local_player_.IsValid() || is_awaiting_display_confirmation() ||
        !is_available(setting)) {
        return;
    }

    auto const& descriptor{game_setting_descriptor(setting)};
    if (!player_settings().set_setting(draft_, setting, value, supported_resolutions())) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Rejected invalid value for game setting %s"),
               *to_fstring(descriptor.name));
        return;
    }

    settings_changed.Broadcast();
}

auto UOptionsDraft::descriptors(GameSettingCategory const category) const
    -> TArray<GameSettingDescriptor const*> {
    TArray<GameSettingDescriptor const*> result;
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category) {
            result.Add(&descriptor);
        }
    }

    return result;
}

auto UOptionsDraft::options(GameSetting const setting) const -> TArray<FGameSettingOption> {
    auto const provider{game_setting_descriptor(setting).options_provider};
    if (!editing_local_player_.IsValid()) {
        return {};
    }

    auto const choices{
        player_settings().choices(draft_.pending(), provider, supported_resolutions())};
    TArray<FGameSettingOption> result;
    result.Reserve(static_cast<int32>(choices.size()));
    for (auto const& choice : choices) {
        auto const* number{std::get_if<float>(&choice.value)};
        auto label{provider == GameSettingOptionProvider::FrameRateLimits && number && *number > 0.f
                       ? FText::AsNumber(static_cast<int32>(*number))
                       : FText::FromString(to_fstring(choice.label))};
        result.Add({choice.value, MoveTemp(label)});
    }

    return result;
}

auto UOptionsDraft::is_available(GameSetting const setting) const -> bool {
    return editing_local_player_.IsValid() &&
           player_settings().is_available(draft_.pending(), setting);
}

auto UOptionsDraft::is_dirty() const -> bool {
    return draft_.is_dirty() || input_is_dirty();
}

auto UOptionsDraft::is_at_defaults(GameSettingCategory const category) const -> bool {
    if (category == GameSettingCategory::Controls) {
        auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
        if (bindings.ContainsByPredicate(
                [](auto const& binding) { return binding.is_modified(); })) {
            return false;
        }
    }

    return draft_.is_at_defaults(category);
}

auto UOptionsDraft::is_awaiting_display_confirmation() const -> bool {
    return confirmation_deadline_ != 0.0;
}

auto UOptionsDraft::display_confirmation_seconds_remaining() const -> int32 {
    return FMath::Max(0, FMath::CeilToInt(confirmation_deadline_ - FPlatformTime::Seconds()));
}

/* **************************************** */
// Settings and display helpers
/* **************************************** */
void UOptionsDraft::finish_display_confirmation() {
    confirmation_deadline_ = 0.0;
    if (display_confirmation_ticker_.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(display_confirmation_ticker_);
        display_confirmation_ticker_.Reset();
    }

    display_confirmation_changed.Broadcast(false);
    settings_changed.Broadcast();
}

auto UOptionsDraft::tick_display_confirmation(float const delta_seconds) -> bool {
    static_cast<void>(delta_seconds);
    if (!is_awaiting_display_confirmation()) {
        return false;
    }

    if (FPlatformTime::Seconds() >= confirmation_deadline_) {
        revert_display_changes();
        return is_awaiting_display_confirmation();
    }

    display_confirmation_changed.Broadcast(true);
    return true;
}

/* **************************************** */
// Control bindings
/* **************************************** */

auto UOptionsDraft::all_control_bindings() const -> TArray<FControlBindingView> {
    TArray<FControlBindingView> result;

    for (auto const& control : input::flight_controls()) {
        auto const key{input::bound_key(control, draft_.pending().controls)};
        auto const group{
            control.group == input::BindingGroup::Utility  ? EControlBindingGroup::Utility
            : control.group == input::BindingGroup::Combat ? EControlBindingGroup::Combat
                                                           : EControlBindingGroup::Flight};
        auto const device{is_gamepad_mapping(control.mapping)
                              ? FHardwareDeviceIdentifier::DefaultGamepad
                              : FHardwareDeviceIdentifier::DefaultKeyboardAndMouse};
        result.Add(FControlBindingView{
            .address = {.mapping = control.mapping},
            .display_name = FText::FromString(to_fstring(control.label)),
            .display_category = control_binding_group_label(group),
            .display_group = group,
            .display_order = control.display_order,
            .device_type = device.PrimaryDeviceType,
            .current_key = binding_key(key),
            .default_key = binding_key(control.default_key),
        });
    }

    result.Sort([](auto const& left, auto const& right) {
        if (left.address.profile_index != right.address.profile_index) {
            return left.address.profile_index < right.address.profile_index;
        }

        if (left.display_group != right.display_group) {
            return static_cast<uint8>(left.display_group) < static_cast<uint8>(right.display_group);
        }

        if (left.display_order != right.display_order) {
            return left.display_order < right.display_order;
        }

        if (left.device_type != right.device_type) {
            return static_cast<uint8>(left.device_type) < static_cast<uint8>(right.device_type);
        }

        if (left.address.mapping != right.address.mapping) {
            return left.address.mapping < right.address.mapping;
        }

        return left.address.slot < right.address.slot;
    });

    return result;
}

auto UOptionsDraft::control_bindings(EHardwareDevicePrimaryType const device_type) const
    -> TArray<FControlBindingView> {
    return bindings_.FilterByPredicate([device_type](auto const& binding) {
        return control_binding_matches_device(binding, device_type);
    });
}

auto UOptionsDraft::binding_conflicts(FControlBindingAddress const& address, FKey const& key) const
    -> TArray<FControlBindingView> {
    if (!key.IsValid()) {
        return {};
    }

    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const native{native_control_bindings(bindings)};
    TArray<FControlBindingView> result;
    for (auto const index : control_binding_conflicts(native, address, native_binding_key(key))) {
        result.Add(bindings[static_cast<int32>(index)]);
    }

    return result;
}

auto UOptionsDraft::chord_binding_conflicts(FControlBindingAddress const& address,
                                            FKey const& activator_key,
                                            FKey const& action_key) const
    -> TArray<FControlBindingView> {
    if (!activator_key.IsValid() || !action_key.IsValid()) {
        return {};
    }

    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const native{native_control_bindings(bindings)};
    TArray<FControlBindingView> result;
    for (auto const index : control_chord_binding_conflicts(
             native, address, native_binding_key(activator_key), native_binding_key(action_key))) {
        result.Add(bindings[static_cast<int32>(index)]);
    }

    return result;
}

auto UOptionsDraft::set_control_binding(FControlBindingAddress const& address,
                                        FKey const& key,
                                        bool const replace_conflicts) -> bool {
    if (!editing_local_player_.IsValid() || !key.IsValid()) {
        return false;
    }

    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{bindings.FindByPredicate(
        [&address](auto const& binding) { return binding.address == address; })};
    if (target == nullptr ||
        (target->device_type == EHardwareDevicePrimaryType::Gamepad) != key.IsGamepadKey()) {
        return false;
    }

    if (!input::accepts_binding(address.mapping, native_binding_key(key))) {
        return false;
    }
    auto const conflicts{binding_conflicts(address, key)};
    if (!conflicts.IsEmpty() && !replace_conflicts) {
        return false;
    }

    for (auto const& conflict : conflicts) {
        set_draft_key(conflict.address, EKeys::Invalid);
    }

    set_draft_key(address, key);
    settings_changed.Broadcast();
    return true;
}

auto UOptionsDraft::set_control_chord(FControlBindingAddress const& address,
                                      FKey const& activator_key,
                                      FKey const& action_key,
                                      bool const replace_conflicts) -> bool {
    if (!editing_local_player_.IsValid() || !can_hold_chord_key(activator_key) ||
        !action_key.IsValid() || activator_key == action_key) {
        return false;
    }

    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{bindings.FindByPredicate(
        [&address](auto const& binding) { return binding.address == address; })};
    if (target == nullptr || !target->chord.IsSet()) {
        return false;
    }

    auto const gamepad{target->device_type == EHardwareDevicePrimaryType::Gamepad};
    if (activator_key.IsGamepadKey() != gamepad || action_key.IsGamepadKey() != gamepad) {
        return false;
    }

    auto const conflicts{chord_binding_conflicts(address, activator_key, action_key)};
    if (!conflicts.IsEmpty() && !replace_conflicts) {
        return false;
    }

    for (auto const& conflict : conflicts) {
        set_draft_key(conflict.address, EKeys::Invalid);
    }

    set_draft_key(target->chord->address, activator_key);
    set_draft_key(address, action_key);
    settings_changed.Broadcast();
    return true;
}

auto UOptionsDraft::clear_control_binding(FControlBindingAddress const& address) -> bool {
    if (!editing_local_player_.IsValid()) {
        return false;
    }

    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{bindings.FindByPredicate(
        [&address](auto const& binding) { return binding.address == address; })};
    if (target == nullptr || !target->current_key.IsValid()) {
        return false;
    }

    set_draft_key(address, EKeys::Invalid);
    settings_changed.Broadcast();
    return true;
}

auto UOptionsDraft::reset_control_binding(FControlBindingAddress const& address) -> bool {
    if (!editing_local_player_.IsValid()) {
        return false;
    }

    auto const* binding{
        bindings_.FindByPredicate([&](auto const& item) { return item.address == address; })};
    if (!binding) {
        return false;
    }

    set_draft_key(address, binding->default_key);
    settings_changed.Broadcast();
    return true;
}

auto UOptionsDraft::reset_all_control_bindings() -> bool {
    if (!editing_local_player_.IsValid()) {
        return false;
    }

    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    for (auto const& binding : bindings) {
        set_draft_key(binding.address, binding.default_key);
        if (binding.chord) {
            set_draft_key(binding.chord->address, binding.chord->default_key);
        }
    }

    settings_changed.Broadcast();
    return true;
}

/* **************************************** */
// Input edit state
/* **************************************** */
void UOptionsDraft::set_draft_key(FControlBindingAddress const& address, FKey const& key) {
    for (auto& binding : bindings_) {
        if (binding.address == address) {
            binding.current_key = key;
        }

        if (binding.chord && binding.chord->address == address) {
            binding.chord->current_key = key;
        }
    }
}
auto UOptionsDraft::input_is_dirty() const -> bool {
    for (int32 i{}; i < bindings_.Num(); ++i) {
        auto const& binding{bindings_[i]};
        auto const& original{original_bindings_[i]};
        if (binding.current_key != original.current_key ||
            (binding.chord && binding.chord->current_key != original.chord->current_key)) {
            return true;
        }
    }

    return false;
}
} // namespace ioj
