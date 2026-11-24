#include "SpaceGame/settings/GameSettingsSubsystem.h"
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "GameHost.h"
#else
#include "GameFramework/GameUserSettings.h"

#include <filesystem>
#include <optional>
#endif

#include "GameplayTagContainer.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "SpaceGame/input/SpaceGameInputUserSettings.h"
#include "SpaceGame/settings/ControlBindingAddress.h"
#include "SpaceGame/settings/ControlBindings.h"
#include "SpaceGame/settings/ControlBindingView.h"
#include "SpaceGame/settings/ControlChordBindingView.h"
#include "SpaceGame/settings/GameSettingOption.h"
#include <ioj/settings/application_settings.h>
#include <ioj/settings/control_bindings.h>
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_setting_value.h>
#include <ioj/settings/game_settings.h>
#include <ioj/settings/game_settings_apply_result.h>
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
auto load_instance_settings(PlayerSettings& settings) -> SettingsResult {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    settings = take_prepared_settings();
    return {};
#else
    auto const scripts{
        std::filesystem::path{*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())} /
        "LevelScripts"};
    auto const desktop{GEngine->GetGameUserSettings()->GetDesktopResolution()};
    return settings.load(scripts,
                         scripts / "Settings/user-settings.scm",
                         {desktop.X > 0 ? desktop.X : 1920, desktop.Y > 0 ? desktop.Y : 1080},
                         std::nullopt);
#endif
}

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
void UGameSettingsSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    if (auto result{load_instance_settings(player_settings_)}; !result) {
        report_settings_error(result.error(), false);
        return;
    }

    if (auto result{player_settings_.activate(backend_)}; !result) {
        report_settings_error(result.error(), false);
    }
}

void UGameSettingsSubsystem::Deinitialize() {
    if (is_awaiting_display_confirmation()) {
        revert_display_changes();
    }
    Super::Deinitialize();
}

/* **************************************** */
// Edit sessions and display confirmation
/* **************************************** */
void UGameSettingsSubsystem::begin_edit(ULocalPlayer* const local_player) {
    if (is_awaiting_display_confirmation()) {
        return;
    }

    editing_local_player_ = local_player;
    if (auto result{player_settings_.begin_edit(backend_)}; !result) {
        cancel();
        report_settings_error(result.error(), true);
        return;
    }

    capture_input_edit_state();
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::cancel() {
    if (!player_settings_.session().is_editing() || is_awaiting_display_confirmation()) {
        return;
    }

    auto const before{settings_state()};
    restore_input_edit_state();
    player_settings_.cancel();
    for (auto const& descriptor : game_setting_descriptors()) {
        if (game_setting_value(before, descriptor.id) != value(descriptor.id)) {
            setting_changed.Broadcast(descriptor.id);
        }
    }
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::apply() {
    if (!player_settings_.session().is_editing() || is_awaiting_display_confirmation()) {
        return;
    }

    auto const bindings_changed{input_is_dirty()};
    auto const result{player_settings_.apply(backend_, FPlatformTime::Seconds(), bindings_changed)};
    if (!result) {
        report_settings_error(result.error(), true);
        return;
    }
    if (*result == GameSettingsApplyResult::Ignored) {
        return;
    }

    if (bindings_changed) {
        if (auto* const input_settings{input_user_settings()}) {
            input_settings->ApplySettings();
            input_settings->AsyncSaveSettings();
        }
    }

    capture_input_edit_state();
    if (*result == GameSettingsApplyResult::Applied) {
        settings_changed.Broadcast();
        return;
    }

    display_confirmation_ticker_ = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &ThisClass::tick_display_confirmation), 0.25f);
    display_confirmation_changed.Broadcast(true);
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::reset_category(GameSettingCategory const category) {
    auto const before{settings_state()};
    if (!player_settings_.reset_category(category)) {
        return;
    }
    if (category == GameSettingCategory::Controls) {
        reset_all_control_bindings();
    }
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category &&
            descriptor.apply_mode == SettingApplyMode::Immediate &&
            game_setting_value(before, descriptor.id) != value(descriptor.id)) {
            setting_changed.Broadcast(descriptor.id);
        }
    }
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::confirm_display_changes() {
    if (!is_awaiting_display_confirmation()) {
        return;
    }

    if (auto result{player_settings_.confirm_display(backend_)}; !result) {
        report_settings_error(result.error(), true);
    }

    if (!is_awaiting_display_confirmation()) {
        finish_display_confirmation();
    }
}

void UGameSettingsSubsystem::revert_display_changes() {
    if (!is_awaiting_display_confirmation()) {
        return;
    }

    if (auto result{player_settings_.revert_display(backend_)}; !result) {
        report_settings_error(result.error(), true);
    }

    if (!is_awaiting_display_confirmation()) {
        finish_display_confirmation();
    }
}

/* **************************************** */
// Settings state
/* **************************************** */
auto UGameSettingsSubsystem::application_settings(UGameInstance const* instance)
    -> ApplicationSettings const& {
    auto const* settings{UGameInstance::GetSubsystem<UGameSettingsSubsystem>(instance)};
    check(settings);
    return settings->player_settings_.application();
}

auto UGameSettingsSubsystem::settings_state() const -> GameSettingsState const& {
    return player_settings_.session().settings_state();
}

auto UGameSettingsSubsystem::applied_state() const -> GameSettingsState const& {
    return player_settings_.session().applied_state();
}

auto UGameSettingsSubsystem::default_state() const -> GameSettingsState const& {
    return player_settings_.session().default_state();
}

auto UGameSettingsSubsystem::value(GameSetting const setting) const -> GameSettingValue {
    return player_settings_.session().value(setting);
}

void UGameSettingsSubsystem::set_setting(GameSetting const setting, GameSettingValue const& value) {
    if (!player_settings_.session().is_editing() || is_awaiting_display_confirmation() ||
        !is_available(setting)) {
        return;
    }
    auto const& descriptor{game_setting_descriptor(setting)};
    if (!player_settings_.set_setting(
            setting, value, backend_.supported_resolutions(descriptor.options_provider))) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Rejected invalid value for game setting %s"),
               *to_fstring(descriptor.name));
        return;
    }
    setting_changed.Broadcast(setting);
    settings_changed.Broadcast();
}

auto UGameSettingsSubsystem::descriptors(GameSettingCategory const category) const
    -> TArray<GameSettingDescriptor const*> {
    TArray<GameSettingDescriptor const*> result;
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category) {
            result.Add(&descriptor);
        }
    }
    return result;
}

auto UGameSettingsSubsystem::options(GameSetting const setting) const
    -> TArray<FGameSettingOption> {
    auto const provider{game_setting_descriptor(setting).options_provider};
    auto const choices{
        player_settings_.choices(provider, backend_.supported_resolutions(provider))};
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

auto UGameSettingsSubsystem::is_available(GameSetting const setting) const -> bool {
    return player_settings_.is_available(setting);
}

auto UGameSettingsSubsystem::is_dirty() const -> bool {
    return player_settings_.session().is_dirty() || input_is_dirty();
}

auto UGameSettingsSubsystem::is_dirty(GameSettingCategory const category) const -> bool {
    return player_settings_.session().is_dirty(category) ||
           (category == GameSettingCategory::Controls && input_is_dirty());
}

auto UGameSettingsSubsystem::is_at_defaults(GameSettingCategory const category) const -> bool {
    if (category == GameSettingCategory::Controls) {
        auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
        if (bindings.ContainsByPredicate(
                [](auto const& binding) { return binding.is_modified(); })) {
            return false;
        }
    }
    return player_settings_.session().is_at_defaults(category);
}

auto UGameSettingsSubsystem::is_awaiting_display_confirmation() const -> bool {
    return player_settings_.session().is_awaiting_display_confirmation();
}

auto UGameSettingsSubsystem::display_confirmation_seconds_remaining() const -> int32 {
    return player_settings_.session().display_confirmation_seconds_remaining(
        FPlatformTime::Seconds());
}

/* **************************************** */
// Settings and display helpers
/* **************************************** */
void UGameSettingsSubsystem::finish_display_confirmation() {
    if (display_confirmation_ticker_.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(display_confirmation_ticker_);
        display_confirmation_ticker_.Reset();
    }
    display_confirmation_changed.Broadcast(false);
    settings_changed.Broadcast();
}

auto UGameSettingsSubsystem::tick_display_confirmation(float const delta_seconds) -> bool {
    static_cast<void>(delta_seconds);
    if (!is_awaiting_display_confirmation()) {
        return false;
    }
    if (player_settings_.session().display_confirmation_expired(FPlatformTime::Seconds())) {
        revert_display_changes();
        return is_awaiting_display_confirmation();
    }
    display_confirmation_changed.Broadcast(true);
    return true;
}

auto UGameSettingsSubsystem::input_user_settings() const -> USpaceGameInputUserSettings* {
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

auto UGameSettingsSubsystem::all_control_bindings() const -> TArray<FControlBindingView> {
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

auto UGameSettingsSubsystem::control_bindings(EHardwareDevicePrimaryType const device_type) const
    -> TArray<FControlBindingView> {
    auto* const settings{input_user_settings()};
    if (settings == nullptr) {
        return {};
    }

    auto bindings{all_control_bindings().FilterByPredicate([settings](auto const& binding) {
        return binding.address.profile_id == settings->GetActiveKeyProfileId();
    })};

    return bindings.FilterByPredicate([device_type](auto const& binding) {
        return control_binding_matches_device(binding, device_type);
    });
}

auto UGameSettingsSubsystem::binding_conflicts(FControlBindingAddress const& address,
                                               FKey const& key) const
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

auto UGameSettingsSubsystem::chord_binding_conflicts(FControlBindingAddress const& address,
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

auto UGameSettingsSubsystem::map_control_binding(USpaceGameInputUserSettings& settings,
                                                 FControlBindingAddress const& address,
                                                 FKey const& key,
                                                 bool const defer_change_broadcast) const -> bool {
    FMapPlayerKeyArgs arguments{};
    arguments.MappingName = address.mapping_name;
    arguments.Slot = address.slot;
    arguments.NewKey = key;
    arguments.HardwareDeviceId = address.hardware_device_id;
    arguments.ProfileIdString = address.profile_id;
    arguments.bDeferOnSettingsChangedBroadcast = defer_change_broadcast;

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

auto UGameSettingsSubsystem::unmap_control_binding(USpaceGameInputUserSettings& settings,
                                                   FControlBindingAddress const& address) const
    -> bool {
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

auto UGameSettingsSubsystem::set_control_binding(FControlBindingAddress const& address,
                                                 FKey const& key,
                                                 bool const replace_conflicts) -> bool {
    auto* const settings{input_user_settings()};
    if (!player_settings_.session().is_editing() || settings == nullptr || !key.IsValid() ||
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

    TArray<FControlBindingView> unmapped_conflicts;
    unmapped_conflicts.Reserve(conflicts.Num());
    for (auto const& conflict : conflicts) {
        if (!map_control_binding(*settings, conflict.address, EKeys::Invalid, false)) {
            for (auto const& unmapped : unmapped_conflicts) {
                static_cast<void>(
                    map_control_binding(*settings, unmapped.address, unmapped.current_key, false));
            }
            return false;
        }
        unmapped_conflicts.Add(conflict);
    }

    if (!map_control_binding(*settings, address, key, false)) {
        for (auto const& unmapped : unmapped_conflicts) {
            static_cast<void>(
                map_control_binding(*settings, unmapped.address, unmapped.current_key, false));
        }
        return false;
    }

    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::set_control_chord(FControlBindingAddress const& address,
                                               FKey const& activator_key,
                                               FKey const& action_key,
                                               bool const replace_conflicts) -> bool {
    auto* const settings{input_user_settings()};
    if (!player_settings_.session().is_editing() || settings == nullptr ||
        !can_hold_chord_key(activator_key) || !action_key.IsValid() ||
        activator_key == action_key || address.profile_id != settings->GetActiveKeyProfileId()) {
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

    TArray<FControlBindingView> changed;
    changed.Reserve(conflicts.Num() + 2);
    for (auto const& conflict : conflicts) {
        if (!map_control_binding(*settings, conflict.address, EKeys::Invalid, true)) {
            for (auto const& original : changed) {
                static_cast<void>(
                    map_control_binding(*settings, original.address, original.current_key, true));
            }
            return false;
        }
        changed.Add(conflict);
    }

    auto apply_target = [this, settings, &changed](FControlBindingAddress const& target_address,
                                                   FKey const& old_key,
                                                   FKey const& new_key) {
        if (old_key == new_key) {
            return true;
        }
        if (!map_control_binding(*settings, target_address, new_key, true)) {
            for (auto const& original : changed) {
                static_cast<void>(
                    map_control_binding(*settings, original.address, original.current_key, true));
            }
            return false;
        }
        changed.Add(FControlBindingView{.address = target_address, .current_key = old_key});
        return true;
    };
    if (!apply_target(target->chord->address, target->chord->current_key, activator_key) ||
        !apply_target(address, target->current_key, action_key)) {
        return false;
    }

    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::clear_control_binding(FControlBindingAddress const& address) -> bool {
    auto* const settings{input_user_settings()};
    if (!player_settings_.session().is_editing() || settings == nullptr ||
        address.profile_id != settings->GetActiveKeyProfileId()) {
        return false;
    }
    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{bindings.FindByPredicate(
        [&address](auto const& binding) { return binding.address == address; })};
    if (target == nullptr || !target->current_key.IsValid()) {
        return false;
    }

    FMapPlayerKeyArgs arguments{};
    arguments.MappingName = address.mapping_name;
    arguments.Slot = address.slot;
    arguments.NewKey = EKeys::Invalid;
    arguments.HardwareDeviceId = address.hardware_device_id;
    arguments.ProfileIdString = address.profile_id;
    FGameplayTagContainer failure_reason;
    settings->MapPlayerKey(arguments, failure_reason);
    if (!failure_reason.IsEmpty()) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Could not clear control '%s': %s"),
               *address.mapping_name.ToString(),
               *failure_reason.ToStringSimple());
        return false;
    }
    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::reset_control_binding(FControlBindingAddress const& address) -> bool {
    auto* const settings{input_user_settings()};
    if (!player_settings_.session().is_editing() || settings == nullptr) {
        return false;
    }
    if (!unmap_control_binding(*settings, address)) {
        return false;
    }
    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::reset_all_control_bindings() -> bool {
    auto* const settings{input_user_settings()};
    if (!player_settings_.session().is_editing() || settings == nullptr) {
        return false;
    }
    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    for (auto const& binding : bindings) {
        if (!unmap_control_binding(*settings, binding.address)) {
            return false;
        }
    }
    settings_changed.Broadcast();
    return true;
}

/* **************************************** */
// Input edit state
/* **************************************** */
void UGameSettingsSubsystem::capture_input_edit_state() {
    applied_control_bindings_ = native_control_bindings(all_control_bindings());
}

void UGameSettingsSubsystem::restore_input_edit_state() {
    auto* const settings{input_user_settings()};
    if (settings == nullptr) {
        return;
    }

    for (auto const& binding : applied_control_bindings_) {
        if (binding.current_key == binding.default_key) {
            static_cast<void>(
                unmap_control_binding(*settings, engine_control_binding_address(binding.address)));
        } else {
            static_cast<void>(map_control_binding(*settings,
                                                  engine_control_binding_address(binding.address),
                                                  FKey{FName{*to_fstring(binding.current_key)}},
                                                  true));
        }
    }
    settings->ApplySettings();
}

auto UGameSettingsSubsystem::input_is_dirty() const -> bool {
    return control_bindings_changed(native_control_bindings(all_control_bindings()),
                                    applied_control_bindings_);
}

} // namespace ioj
