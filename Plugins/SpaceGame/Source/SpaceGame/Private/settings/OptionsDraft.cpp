#include "SpaceGame/settings/OptionsDraft.h"

#include "GameplayTagContainer.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "SpaceGame/input/SpaceGameInputUserSettings.h"
#include "SpaceGame/settings/ControlBindingAddress.h"
#include "SpaceGame/settings/ControlBindings.h"
#include "SpaceGame/settings/ControlBindingView.h"
#include "SpaceGame/settings/ControlChordBindingView.h"
#include "SpaceGame/settings/GameSettingOption.h"
#include "SpaceGame/system/GameSubsystem.h"
#include <ioj/settings/application_settings.h>
#include <ioj/settings/control_bindings.h>
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_setting_value.h>
#include <ioj/settings/game_settings.h>
#include <ioj/settings/player_settings.h>
#include <ioj/settings/setting_apply_mode.h>

#include <SandboxCoreEngine/strings.h>

#include "Containers/Array.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/InputSettings.h"
#include "HAL/Platform.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Optional.h"
#include "Subsystems/SubsystemCollection.h"
#include "Templates/Casts.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Object.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include <Engine/GameInstance.h>
#include <Misc/App.h>
#include <Misc/MessageDialog.h>
#include <Misc/Paths.h>

#include <string>
#include <variant>

namespace ioj {
namespace {
void report_settings_error(std::string const& error, bool interactive) {
    auto const message{FText::FromString(to_fstring(error))};
    UE_LOG(LogTemp, Error, TEXT("%s"), *message.ToString());
    if (interactive && !FApp::IsUnattended() && !GIsAutomationTesting) {
        FMessageDialog::Open(EAppMsgType::Ok, message);
    }
}
}

/* **************************************** */
// Lifecycle
/* **************************************** */
void UOptionsDraft::begin_edit(UGameSubsystem& game,
                               ULocalPlayer* player,
                               GameSettingCategory category) {
    check(!is_awaiting_display_confirmation());
    auto& settings{game.player_settings()};
    if (!settings.loaded()) {
        report_settings_error("Options are unavailable because the profile could not be loaded",
                              true);
        return;
    }
    game_ = &game;
    category_ = category;
    editing_local_player_ = player;
    if (category == GameSettingCategory::Video && settings.launch_display() &&
        settings.launch_display() != LaunchDisplayMode::Headless) {
        if (auto display{video_.inspect_display()}; display) {
            settings.set_running_display(*display);
        } else {
            report_settings_error("Reading window placement failed: " + display.error(), true);
            game_.Reset();
            return;
        }
    }
    draft_.begin(settings.applied(), settings.defaults());
    capture_input_edit_state();
    settings_changed.Broadcast();
}

void UOptionsDraft::cancel() {
    if (is_awaiting_display_confirmation()) {
        revert_display_changes();
    }
    draft_.cancel();
    bindings_ = original_bindings_;
    settings_changed.Broadcast();
}

auto UOptionsDraft::apply() -> bool {
    if (!game_.IsValid() || is_awaiting_display_confirmation() || !is_dirty()) {
        return false;
    }
    auto const& pending{draft_.pending()};
    auto const& applied{draft_.applied()};
    if (category_ == GameSettingCategory::Video &&
        (pending.window_mode != applied.window_mode || pending.resolution != applied.resolution)) {
        if (auto result{video_.apply_display(pending)}; !result) {
            report_settings_error("Applying display settings failed: " + result.error(), true);
            return false;
        }
        confirmation_deadline_ = FPlatformTime::Seconds() + 15.0;
        display_confirmation_ticker_ = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &ThisClass::tick_display_confirmation), 0.25f);
        display_confirmation_changed.Broadcast(true);
        settings_changed.Broadcast();
        return true;
    }
    return save();
}

auto UOptionsDraft::save() -> bool {
    bool const bindings_changed{category_ == GameSettingCategory::Controls && input_is_dirty()};
    if (bindings_changed && !apply_input_bindings(bindings_, original_bindings_)) {
        apply_input_bindings(original_bindings_, bindings_);
        report_settings_error("Applying control bindings failed", true);
        return false;
    }
    auto& settings{game_->player_settings()};
    if (auto applied{game_->apply_settings(category_, draft_.pending())}; !applied) {
        if (bindings_changed) {
            apply_input_bindings(original_bindings_, bindings_);
        }
        report_settings_error("Applying settings failed: " + applied.error(), true);
        return false;
    }
    if (bindings_changed && !input_user_settings()->save_profile()) {
        apply_input_bindings(original_bindings_, bindings_);
        auto restored{game_->apply_settings(category_, draft_.applied())};
        report_settings_error(
            "Saving the Enhanced Input profile failed" +
                (restored ? std::string{} : "; restoring settings failed: " + restored.error()),
            true);
        return false;
    }
    if (auto saved{settings.save_category(category_, draft_.pending(), draft_.applied())}; !saved) {
        auto restored{game_->apply_settings(category_, draft_.applied())};
        if (bindings_changed) {
            if (!apply_input_bindings(original_bindings_, bindings_) ||
                !input_user_settings()->save_profile()) {
                saved.error() += "; restoring the Enhanced Input profile failed";
            }
        }
        report_settings_error(
            "Saving settings failed: " + saved.error() +
                (restored ? "" : "; restoring settings failed: " + restored.error()),
            true);
        return false;
    }
    if (bindings_changed) {
        capture_input_edit_state();
    }
    draft_.commit_all();
    settings_changed.Broadcast();
    return true;
}

void UOptionsDraft::reset_category(GameSettingCategory category) {
    if (category != category_ || !game_.IsValid() || is_awaiting_display_confirmation()) {
        return;
    }
    draft_.reset_category(category);
    if (category == GameSettingCategory::Video) {
        auto mode{game_->player_settings().launch_display()};
        if (mode && *mode != LaunchDisplayMode::Preferences) {
            draft_.set_setting(GameSetting::WindowMode, applied_state().window_mode);
        } else if (mode && draft_.pending().window_mode == GameWindowMode::Fullscreen) {
            draft_.set_setting(GameSetting::WindowMode, GameWindowMode::Borderless);
        }
        if (!is_available(GameSetting::Resolution)) {
            draft_.set_setting(GameSetting::Resolution, applied_state().resolution);
        }
    } else if (category == GameSettingCategory::Controls) {
        reset_all_control_bindings();
    }
    settings_changed.Broadcast();
}

void UOptionsDraft::confirm_display_changes() {
    if (!is_awaiting_display_confirmation()) {
        return;
    }
    if (auto result{video_.verify_display()}; !result) {
        report_settings_error("Display verification failed: " + result.error(), true);
        revert_display_changes();
        return;
    }
    if (!save()) {
        if (auto rollback{video_.revert_display()}; rollback) {
            finish_display_confirmation();
        } else {
            report_settings_error("Restoring display failed: " + rollback.error(), true);
        }
        return;
    }
    video_.confirm_display();
    if (game_->player_settings().launch_display()) {
        auto measured{video_.inspect_display()};
        if (measured) {
            game_->player_settings().set_running_display(*measured);
            draft_.begin(game_->player_settings().applied(), game_->player_settings().defaults());
        }
    }
    finish_display_confirmation();
}

void UOptionsDraft::revert_display_changes() {
    if (!is_awaiting_display_confirmation()) {
        return;
    }
    if (auto result{video_.revert_display()}; !result) {
        report_settings_error("Restoring display failed: " + result.error(), true);
        return;
    }
    draft_.cancel();
    finish_display_confirmation();
}

auto UOptionsDraft::settings_state() const -> GameSettingsState const& {
    return draft_.pending();
}

auto UOptionsDraft::applied_state() const -> GameSettingsState const& {
    return draft_.applied();
}

auto UOptionsDraft::default_state() const -> GameSettingsState const& {
    return draft_.defaults();
}

auto UOptionsDraft::value(GameSetting const setting) const -> GameSettingValue {
    return draft_.value(setting);
}

void UOptionsDraft::set_setting(GameSetting const setting, GameSettingValue const& value) {
    if (!game_.IsValid() || is_awaiting_display_confirmation() || !is_available(setting)) {
        return;
    }
    auto const& descriptor{game_setting_descriptor(setting)};
    if (!game_->player_settings().set_setting(
            draft_,
            setting,
            value,
            video_.supported_resolutions(descriptor.options_provider,
                                         draft_.pending().window_mode))) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Rejected invalid value for game setting %s"),
               *to_fstring(descriptor.name));
        return;
    }
    setting_changed.Broadcast(setting);
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
    if (!game_.IsValid()) {
        return {};
    }
    auto const choices{game_->player_settings().choices(
        draft_.pending(),
        provider,
        video_.supported_resolutions(provider, draft_.pending().window_mode))};
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
    return game_.IsValid() && game_setting_descriptor(setting).category == category_ &&
           game_->player_settings().is_available(draft_.pending(), setting);
}

auto UOptionsDraft::is_dirty() const -> bool {
    return draft_.is_dirty() || input_is_dirty();
}

auto UOptionsDraft::is_dirty(GameSettingCategory const category) const -> bool {
    return draft_.is_dirty(category) ||
           (category == GameSettingCategory::Controls && input_is_dirty());
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

auto UOptionsDraft::input_user_settings() const -> USpaceGameInputUserSettings* {
    auto* const local_player{editing_local_player_.Get()};
    auto* const subsystem{
        local_player != nullptr
            ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)
            : nullptr};
    return subsystem != nullptr ? Cast<USpaceGameInputUserSettings>(subsystem->GetUserSettings())
                                : nullptr;
}

/* **************************************** */
// Control bindings
/* **************************************** */

auto UOptionsDraft::all_control_bindings() const -> TArray<FControlBindingView> {
    TArray<FControlBindingView> result;

    auto const* const settings{input_user_settings()};
    if (settings == nullptr) {
        return result;
    }

    for (auto const& profile_pair : settings->GetAllAvailableKeyProfiles()) {
        auto const* const profile{profile_pair.Value.Get()};
        if (!IsValid(profile)) {
            continue;
        }
        for (auto const& row : profile->GetPlayerMappingRows()) {
            for (auto const& mapping : row.Value.Mappings) {
                auto const* const presentation{
                    settings->control_binding_metadata(profile_pair.Key, mapping)};
                if (presentation == nullptr) {
                    continue;
                }
                TOptional<FControlChordBindingView> chord;
                if (auto const* const chord_mapping{
                        settings->chord_mapping_for_mapping(profile_pair.Key, mapping)}) {
                    chord = FControlChordBindingView{
                        .address =
                            {
                                .profile_id = profile_pair.Key,
                                .mapping_name = chord_mapping->GetMappingName(),
                                .hardware_device_id =
                                    chord_mapping->GetHardwareDeviceId().HardwareDeviceIdentifier,
                                .slot = chord_mapping->GetSlot(),
                            },
                        .current_key = chord_mapping->GetCurrentKey(),
                        .default_key = chord_mapping->GetDefaultKey(),
                    };
                }

                result.Add(FControlBindingView{
                    .address =
                        {
                            .profile_id = profile_pair.Key,
                            .mapping_name = mapping.GetMappingName(),
                            .hardware_device_id =
                                mapping.GetHardwareDeviceId().HardwareDeviceIdentifier,
                            .slot = mapping.GetSlot(),
                        },
                    .display_name = mapping.GetDisplayName(),
                    .display_category = mapping.GetDisplayCategory(),
                    .display_group = presentation->group,
                    .display_order = presentation->display_order,
                    .device_type = mapping.GetPrimaryDeviceType(),
                    .current_key = mapping.GetCurrentKey(),
                    .default_key = mapping.GetDefaultKey(),
                    .chord = MoveTemp(chord),
                });
            }
        }
    }

    result.Sort([](auto const& left, auto const& right) {
        if (left.address.profile_id != right.address.profile_id) {
            return left.address.profile_id < right.address.profile_id;
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
        if (left.address.mapping_name != right.address.mapping_name) {
            return left.address.mapping_name.LexicalLess(right.address.mapping_name);
        }
        return static_cast<uint8>(left.address.slot) < static_cast<uint8>(right.address.slot);
    });

    return result;
}

auto UOptionsDraft::control_bindings(EHardwareDevicePrimaryType const device_type) const
    -> TArray<FControlBindingView> {
    auto* const settings{input_user_settings()};
    if (settings == nullptr) {
        return {};
    }

    auto bindings{bindings_.FilterByPredicate([settings](auto const& binding) {
        return binding.address.profile_id == settings->GetActiveKeyProfileId();
    })};

    return bindings.FilterByPredicate([device_type](auto const& binding) {
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
    for (auto const index :
         control_binding_conflicts(native,
                                   native_control_binding_address(address),
                                   to_string(key.GetFName().ToString().ToLower()))) {
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
    for (auto const index :
         control_chord_binding_conflicts(native,
                                         native_control_binding_address(address),
                                         to_string(activator_key.GetFName().ToString().ToLower()),
                                         to_string(action_key.GetFName().ToString().ToLower()))) {
        result.Add(bindings[static_cast<int32>(index)]);
    }
    return result;
}

auto UOptionsDraft::map_control_binding(USpaceGameInputUserSettings& settings,
                                        FControlBindingAddress const& address,
                                        FKey const& key) const -> bool {
    FMapPlayerKeyArgs arguments{};
    arguments.MappingName = address.mapping_name;
    arguments.Slot = address.slot;
    arguments.NewKey = key;
    arguments.HardwareDeviceId = address.hardware_device_id;
    arguments.ProfileIdString = address.profile_id;
    arguments.bDeferOnSettingsChangedBroadcast = false;

    FGameplayTagContainer failure_reason;
    settings.MapPlayerKey(arguments, failure_reason);
    if (!failure_reason.IsEmpty()) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Could not map control '%s': %s"),
               *address.mapping_name.ToString(),
               *failure_reason.ToStringSimple());
        return false;
    }

    return true;
}

auto UOptionsDraft::unmap_control_binding(USpaceGameInputUserSettings& settings,
                                          FControlBindingAddress const& address) const -> bool {
    FMapPlayerKeyArgs arguments{};
    arguments.MappingName = address.mapping_name;
    arguments.Slot = address.slot;
    arguments.HardwareDeviceId = address.hardware_device_id;
    arguments.ProfileIdString = address.profile_id;
    FGameplayTagContainer failure_reason;
    settings.UnMapPlayerKey(arguments, failure_reason);
    if (!failure_reason.IsEmpty()) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Could not reset control '%s': %s"),
               *address.mapping_name.ToString(),
               *failure_reason.ToStringSimple());
        return false;
    }
    return true;
}

auto UOptionsDraft::set_control_binding(FControlBindingAddress const& address,
                                        FKey const& key,
                                        bool const replace_conflicts) -> bool {
    auto* const settings{input_user_settings()};
    if (!game_.IsValid() || settings == nullptr || !key.IsValid() ||
        address.profile_id != settings->GetActiveKeyProfileId()) {
        return false;
    }

    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{bindings.FindByPredicate(
        [&address](auto const& binding) { return binding.address == address; })};
    if (target == nullptr ||
        (target->device_type == EHardwareDevicePrimaryType::Gamepad) != key.IsGamepadKey()) {
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
    auto* const settings{input_user_settings()};
    if (!game_.IsValid() || settings == nullptr || !can_hold_chord_key(activator_key) ||
        !action_key.IsValid() || activator_key == action_key ||
        address.profile_id != settings->GetActiveKeyProfileId()) {
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
    auto* const settings{input_user_settings()};
    if (!game_.IsValid() || settings == nullptr ||
        address.profile_id != settings->GetActiveKeyProfileId()) {
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
    auto* const settings{input_user_settings()};
    if (!game_.IsValid() || settings == nullptr) {
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
    auto* const settings{input_user_settings()};
    if (!game_.IsValid() || settings == nullptr) {
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
void UOptionsDraft::set_draft_key(FControlBindingAddress const& address, FKey key) {
    for (auto& binding : bindings_) {
        if (binding.address == address) {
            binding.current_key = key;
        }
        if (binding.chord && binding.chord->address == address) {
            binding.chord->current_key = key;
        }
    }
}
void UOptionsDraft::capture_input_edit_state() {
    bindings_ = category_ == GameSettingCategory::Controls ? all_control_bindings()
                                                           : TArray<FControlBindingView>{};
    original_bindings_ = bindings_;
}
auto UOptionsDraft::apply_input_bindings(TArray<FControlBindingView> const& bindings,
                                         TArray<FControlBindingView> const& previous) -> bool {
    auto* settings{input_user_settings()};
    if (!settings) {
        return false;
    }
    auto const apply_key = [&](auto const& binding, auto const& before) {
        if (binding.current_key == before.current_key) {
            return true;
        }
        return binding.current_key == binding.default_key
                 ? unmap_control_binding(*settings, binding.address)
                 : map_control_binding(*settings, binding.address, binding.current_key);
    };
    for (int32 index{}; index < bindings.Num(); ++index) {
        auto const& binding{bindings[index]};
        auto const& before{previous[index]};
        if (!apply_key(binding, before) ||
            (binding.chord && !apply_key(*binding.chord, *before.chord))) {
            return false;
        }
    }
    settings->ApplySettings();
    return true;
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
