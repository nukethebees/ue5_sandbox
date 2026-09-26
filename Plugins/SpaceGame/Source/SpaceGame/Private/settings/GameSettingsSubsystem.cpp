#include "SpaceGame/settings/GameSettingsSubsystem.h"

#include "SpaceGame/input/CanonicalShipControls.h"
#include "SpaceGame/input/SpaceGameInputUserSettings.h"
#include "SpaceGame/settings/SpaceGameUserSettings.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "HAL/PlatformTime.h"

namespace ml::ioj {
constexpr double display_confirmation_duration_seconds{15.0};

/* **************************************** */
// Lifecycle
/* **************************************** */
void UGameSettingsSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    edit_state_.begin(backend_.read(), backend_.defaults());
    if (auto* const settings{Cast<USpaceGameUserSettings>(GEngine->GetGameUserSettings())}) {
        flight_model_loadout_ = settings->flight_model_loadout();
    } else {
        flight_model_loadout_ = ::ioj::sim::player::make_default_flight_model_loadout();
    }
    applied_flight_model_loadout_ = flight_model_loadout_;
}

void UGameSettingsSubsystem::Deinitialize() {
    if (display_confirmation_ticker_.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(display_confirmation_ticker_);
        display_confirmation_ticker_.Reset();
    }
    Super::Deinitialize();
}

/* **************************************** */
// Edit sessions and display confirmation
/* **************************************** */
void UGameSettingsSubsystem::begin_edit(ULocalPlayer* const local_player) {
    if (awaiting_display_confirmation_) {
        return;
    }
    backend_.set_local_player(local_player);
    editing_local_player_ = local_player;
    edit_state_.begin(backend_.read(), backend_.defaults());
    flight_model_loadout_ = applied_flight_model_loadout_;
    capture_input_edit_state();
    editing_ = true;
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::cancel() {
    if (!editing_ || awaiting_display_confirmation_) {
        return;
    }
    auto const before{edit_state_.pending()};
    restore_input_edit_state();
    preview_immediate_settings(edit_state_.applied());
    edit_state_.cancel();
    flight_model_loadout_ = applied_flight_model_loadout_;
    flight_model_config_changed.Broadcast();
    editing_ = false;
    for (auto const& descriptor : game_setting_descriptors()) {
        if (game_setting_value(before, descriptor.id) !=
            game_setting_value(edit_state_.pending(), descriptor.id)) {
            setting_changed.Broadcast(descriptor.id);
        }
    }
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
    auto const input_changed{input_is_dirty() ||
                             edit_state_.is_dirty(EGameSettingCategory::Controls)};
    backend_.apply_non_display(pending);
    if (auto* const settings{Cast<USpaceGameUserSettings>(GEngine->GetGameUserSettings())}) {
        settings->set_flight_model_loadout(flight_model_loadout_);
    }
    applied_flight_model_loadout_ = flight_model_loadout_;
    capture_input_edit_state();
    if (input_changed) {
        if (auto* const input_settings{input_user_settings()}) {
            input_settings->ApplySettings();
            input_settings->AsyncSaveSettings();
        }
    }
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
    if (category == EGameSettingCategory::Controls) {
        flight_model_loadout_ = ::ioj::sim::player::make_default_flight_model_loadout();
        flight_model_config_changed.Broadcast();
        reset_all_control_bindings();
    }
    for (auto const& descriptor : game_setting_descriptors()) {
        if (descriptor.category == category &&
            descriptor.apply_mode == ESettingApplyMode::Immediate &&
            game_setting_value(before, descriptor.id) != edit_state_.value(descriptor.id)) {
            backend_.preview_immediate(edit_state_.pending(), descriptor.id);
            setting_changed.Broadcast(descriptor.id);
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

/* **************************************** */
// Settings state
/* **************************************** */
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
    setting_changed.Broadcast(setting);
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
    return edit_state_.is_dirty() || input_is_dirty() ||
           flight_model_loadout_ != applied_flight_model_loadout_;
}

auto UGameSettingsSubsystem::is_dirty(EGameSettingCategory const category) const -> bool {
    return edit_state_.is_dirty(category) ||
           (category == EGameSettingCategory::Controls &&
            (input_is_dirty() || flight_model_loadout_ != applied_flight_model_loadout_));
}

auto UGameSettingsSubsystem::is_at_defaults(EGameSettingCategory const category) const -> bool {
    if (category == EGameSettingCategory::Controls) {
        auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
        auto const has_modified_binding{
            bindings.ContainsByPredicate([](auto const& binding) { return binding.modified; })};
        return edit_state_.is_at_defaults(category) && !has_modified_binding &&
               flight_model_loadout_ == ::ioj::sim::player::make_default_flight_model_loadout();
    }
    return edit_state_.is_at_defaults(category);
}

auto UGameSettingsSubsystem::flight_model_profile(EShipControlScope const scope) const
    -> ::ioj::sim::player::FlightModelProfile const& {
    check(scope != EShipControlScope::General);
    return ::ioj::sim::player::flight_model_profile(flight_model_loadout_,
                                                    flight_model_slot(scope));
}

auto UGameSettingsSubsystem::flight_model_loadout() const
    -> ::ioj::sim::player::FlightModelLoadout const& {
    return flight_model_loadout_;
}

auto UGameSettingsSubsystem::set_flight_model_profile(
    EShipControlScope const scope, ::ioj::sim::player::FlightModelProfile profile) -> bool {
    if (!editing_ || awaiting_display_confirmation_ || scope == EShipControlScope::General) {
        return false;
    }
    auto const slot{flight_model_slot(scope)};
    auto const preset{
        ::ioj::sim::player::flight_model_profile(flight_model_loadout_, slot).base_preset};
    if (profile.base_preset != preset ||
        !::ioj::sim::player::matches_authored_flight_model_topology(profile.config, preset) ||
        !::ioj::sim::player::validate_flight_model_config(profile.config)) {
        return false;
    }
    profile.customized = true;
    ::ioj::sim::player::flight_model_profile(flight_model_loadout_, slot) = profile;
    flight_model_config_changed.Broadcast();
    settings_changed.Broadcast();
    return true;
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

/* **************************************** */
// Settings and display helpers
/* **************************************** */
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
                    .scope = presentation->scope,
                    .device_type = mapping.GetPrimaryDeviceType(),
                    .current_key = mapping.GetCurrentKey(),
                    .default_key = mapping.GetDefaultKey(),
                    .chord = MoveTemp(chord),
                    .modified = mapping.IsCustomized(),
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

auto UGameSettingsSubsystem::control_bindings(EHardwareDevicePrimaryType const device_type,
                                              EShipControlScope const scope) const
    -> TArray<FControlBindingView> {
    return control_bindings(device_type).FilterByPredicate([scope](auto const& binding) {
        return binding.scope == scope;
    });
}

auto UGameSettingsSubsystem::binding_conflicts(FControlBindingAddress const& address,
                                               FKey const key) const
    -> TArray<FControlBindingView> {
    if (!key.IsValid()) {
        return {};
    }

    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{bindings.FindByPredicate(
        [&address](auto const& binding) { return binding.address == address; })};
    if (target == nullptr) {
        return {};
    }

    return bindings.FilterByPredicate([&address, key, target](auto const& binding) {
        auto const same_chord_scope{(!target->chord.IsSet() && !binding.chord.IsSet()) ||
                                    (target->chord.IsSet() && binding.chord.IsSet() &&
                                     target->chord->address == binding.chord->address)};
        auto const same_runtime_scope{target->scope == EShipControlScope::General ||
                                      binding.scope == EShipControlScope::General ||
                                      target->scope == binding.scope};
        return same_runtime_scope && same_chord_scope &&
               binding.address.profile_id == address.profile_id &&
               binding.address.hardware_device_id == address.hardware_device_id &&
               binding.address != address && binding.current_key == key;
    });
}

auto UGameSettingsSubsystem::chord_binding_conflicts(FControlBindingAddress const& address,
                                                     FKey const activator_key,
                                                     FKey const action_key) const
    -> TArray<FControlBindingView> {
    if (!activator_key.IsValid() || !action_key.IsValid()) {
        return {};
    }
    auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    auto const* const target{bindings.FindByPredicate(
        [&address](auto const& binding) { return binding.address == address; })};
    if (target == nullptr || !target->chord.IsSet()) {
        return {};
    }

    TArray<FControlBindingView> result;
    for (auto const& binding : bindings) {
        if (binding.address.profile_id != address.profile_id ||
            binding.address.hardware_device_id != address.hardware_device_id ||
            binding.address == address || binding.address == target->chord->address) {
            continue;
        }
        auto const conflicts_with_activator{!binding.chord.IsSet() &&
                                            binding.current_key == activator_key};
        auto const conflicts_with_action{binding.chord.IsSet() &&
                                         binding.chord->address == target->chord->address &&
                                         binding.current_key == action_key};
        if (conflicts_with_activator || conflicts_with_action) {
            result.Add(binding);
        }
    }
    return result;
}

auto UGameSettingsSubsystem::map_control_binding(USpaceGameInputUserSettings& settings,
                                                 FControlBindingAddress const& address,
                                                 FKey const key,
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
                                                 FKey const key,
                                                 bool const replace_conflicts) -> bool {
    auto* const settings{input_user_settings()};
    if (!editing_ || settings == nullptr || !key.IsValid() ||
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
                                               FKey const activator_key,
                                               FKey const action_key,
                                               bool const replace_conflicts) -> bool {
    auto* const settings{input_user_settings()};
    if (!editing_ || settings == nullptr || !can_hold_chord_key(activator_key) ||
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
                                                   FKey const old_key,
                                                   FKey const new_key) {
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
    if (!editing_ || settings == nullptr ||
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
    if (!editing_ || settings == nullptr) {
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
    if (!editing_ || settings == nullptr) {
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
    applied_control_bindings_ = all_control_bindings();
}

void UGameSettingsSubsystem::restore_input_edit_state() {
    auto* const settings{input_user_settings()};
    if (settings == nullptr) {
        return;
    }

    for (auto const& binding : applied_control_bindings_) {
        if (binding.current_key == binding.default_key) {
            static_cast<void>(unmap_control_binding(*settings, binding.address));
        } else {
            static_cast<void>(
                map_control_binding(*settings, binding.address, binding.current_key, true));
        }
    }
    settings->ApplySettings();
}

auto UGameSettingsSubsystem::input_is_dirty() const -> bool {
    auto const current{all_control_bindings()};
    if (current.Num() != applied_control_bindings_.Num()) {
        return true;
    }
    auto const count{current.Num()};
    for (int32 index{}; index < count; ++index) {
        if (current[index].address != applied_control_bindings_[index].address ||
            current[index].current_key != applied_control_bindings_[index].current_key) {
            return true;
        }
    }
    return false;
}

} // namespace ml::ioj
