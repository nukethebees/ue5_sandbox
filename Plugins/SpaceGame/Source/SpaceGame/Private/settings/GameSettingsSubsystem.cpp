#include "SpaceGame/settings/GameSettingsSubsystem.h"

#include "Containers/Ticker.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "SpaceGame/input/ControlProfiles.h"
#include "SpaceGame/input/SpaceGameInputUserSettings.h"

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

void UGameSettingsSubsystem::begin_edit(ULocalPlayer* const local_player) {
    if (awaiting_display_confirmation_) {
        return;
    }
    backend_.set_local_player(local_player);
    editing_local_player_ = local_player;
    edit_state_.begin(backend_.read(), backend_.defaults());
    capture_input_edit_state();
    editing_ = true;
    settings_changed.Broadcast();
}

void UGameSettingsSubsystem::cancel() {
    if (!editing_ || awaiting_display_confirmation_) {
        return;
    }
    restore_input_edit_state();
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
    capture_input_edit_state();
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
        reset_active_control_profile();
    }
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
    return edit_state_.is_dirty() || input_is_dirty();
}

auto UGameSettingsSubsystem::is_dirty(EGameSettingCategory const category) const -> bool {
    return edit_state_.is_dirty(category) ||
           (category == EGameSettingCategory::Controls && input_is_dirty());
}

auto UGameSettingsSubsystem::is_at_defaults(EGameSettingCategory const category) const -> bool {
    if (category == EGameSettingCategory::Controls) {
        auto const profiles{control_profiles()};
        auto const* const active_profile{
            profiles.FindByPredicate([](auto const& profile) { return profile.active; })};
        if (active_profile != nullptr && active_profile->custom) {
            return edit_state_.is_at_defaults(category);
        }
        auto const bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
        auto const has_modified_binding{
            bindings.ContainsByPredicate([](auto const& binding) { return binding.modified; })};
        return edit_state_.is_at_defaults(category) && !has_modified_binding;
    }
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

auto UGameSettingsSubsystem::input_user_settings() const -> USpaceGameInputUserSettings* {
    auto* const local_player{editing_local_player_.Get()};
    auto* const subsystem{
        local_player != nullptr
            ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)
            : nullptr};
    return subsystem != nullptr ? Cast<USpaceGameInputUserSettings>(subsystem->GetUserSettings())
                                : nullptr;
}

auto UGameSettingsSubsystem::control_profiles() const -> TArray<FControlProfileView> {
    TArray<FControlProfileView> result;
    auto const* const settings{input_user_settings()};
    if (settings == nullptr) {
        return result;
    }

    auto make_view = [settings](FString const& id,
                                UEnhancedPlayerMappableKeyProfile const& profile,
                                bool const custom) {
        auto modified{false};
        for (auto const& row : profile.GetPlayerMappingRows()) {
            for (auto const& mapping : row.Value.Mappings) {
                if (mapping.IsCustomized()) {
                    modified = true;
                    break;
                }
            }
            if (modified) {
                break;
            }
        }
        auto const mapping_profile_id{custom ? settings->custom_key_profile_source_id(id)
                                             : profile.GetProfileIdString()};
        auto const display_name{custom ? settings->custom_key_profile_display_name(id)
                                       : profile.GetProfileDisplayName()};
        return FControlProfileView{
            .id = id,
            .mapping_profile_id = mapping_profile_id.IsEmpty() ? control_profile_definitions()[0].id
                                                               : mapping_profile_id,
            .display_name = display_name.IsEmpty() ? FText::FromString(id) : display_name,
            .active = id == settings->GetActiveKeyProfileId(),
            .modified = modified,
            .custom = custom,
        };
    };

    for (auto const& definition : control_profile_definitions()) {
        auto const* const profile{settings->GetKeyProfileWithId(definition.id)};
        if (!IsValid(profile)) {
            continue;
        }
        result.Add(make_view(definition.id, *profile, false));
    }

    TArray<FControlProfileView> custom_profiles;
    for (auto const& profile_pair : settings->GetAllAvailableKeyProfiles()) {
        auto const* const profile{profile_pair.Value.Get()};
        if (IsValid(profile) && is_custom_control_profile_id(profile_pair.Key)) {
            custom_profiles.Add(make_view(profile_pair.Key, *profile, true));
        }
    }
    custom_profiles.Sort([](auto const& left, auto const& right) {
        auto const name_order{left.display_name.ToString().Compare(right.display_name.ToString())};
        return name_order == 0 ? left.id < right.id : name_order < 0;
    });
    result.Append(MoveTemp(custom_profiles));
    return result;
}

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
                    .device_type = mapping.GetPrimaryDeviceType(),
                    .current_key = mapping.GetCurrentKey(),
                    .default_key = mapping.GetDefaultKey(),
                    .chord = MoveTemp(chord),
                    .modified = mapping.IsCustomized(),
                    .custom_profile = is_custom_control_profile_id(profile_pair.Key),
                });
            }
        }
    }
    result.Sort([](auto const& left, auto const& right) {
        if (left.address.profile_id != right.address.profile_id) {
            return left.address.profile_id < right.address.profile_id;
        }
        if (!left.display_category.EqualTo(right.display_category)) {
            return left.display_category.ToString() < right.display_category.ToString();
        }
        if (!left.display_name.EqualTo(right.display_name)) {
            return left.display_name.ToString() < right.display_name.ToString();
        }
        if (left.device_type != right.device_type) {
            return static_cast<uint8>(left.device_type) < static_cast<uint8>(right.device_type);
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

    TArray<FControlBindingView> missing_device_bindings;
    for (auto const& binding : bindings) {
        auto add_missing_device = [&](EHardwareDevicePrimaryType const missing_device,
                                      FHardwareDeviceIdentifier const& hardware_device) {
            if (bindings.ContainsByPredicate([&binding, missing_device](auto const& candidate) {
                    return candidate.address.mapping_name == binding.address.mapping_name &&
                           candidate.device_type == missing_device;
                }) ||
                missing_device_bindings.ContainsByPredicate(
                    [&binding, missing_device](auto const& candidate) {
                        return candidate.address.mapping_name == binding.address.mapping_name &&
                               candidate.device_type == missing_device;
                    })) {
                return;
            }
            auto missing{binding};
            missing.address.hardware_device_id = hardware_device.HardwareDeviceIdentifier;
            missing.address.slot = EPlayerMappableKeySlot::First;
            missing.device_type = missing_device;
            missing.current_key = EKeys::Invalid;
            missing.default_key = EKeys::Invalid;
            missing.chord.Reset();
            missing.modified = false;
            missing_device_bindings.Add(MoveTemp(missing));
        };

        if (binding.device_type == EHardwareDevicePrimaryType::Gamepad) {
            add_missing_device(EHardwareDevicePrimaryType::KeyboardAndMouse,
                               FHardwareDeviceIdentifier::DefaultKeyboardAndMouse);
        } else {
            add_missing_device(EHardwareDevicePrimaryType::Gamepad,
                               FHardwareDeviceIdentifier::DefaultGamepad);
        }
    }
    bindings.Append(MoveTemp(missing_device_bindings));
    if (device_type == EHardwareDevicePrimaryType::Unspecified) {
        return bindings;
    }
    return bindings.FilterByPredicate(
        [device_type](auto const& binding) { return binding.device_type == device_type; });
}

auto UGameSettingsSubsystem::binding_conflicts(FControlBindingAddress const& address,
                                               FKey const key) const
    -> TArray<FControlBindingView> {
    if (!key.IsValid()) {
        return {};
    }
    return control_bindings(EHardwareDevicePrimaryType::Unspecified)
        .FilterByPredicate([&address, key](auto const& binding) {
            return binding.address.profile_id == address.profile_id &&
                   binding.address.hardware_device_id == address.hardware_device_id &&
                   binding.address != address && binding.current_key == key;
        });
}

auto UGameSettingsSubsystem::set_control_profile(FString const& profile_id) -> bool {
    auto* const settings{input_user_settings()};
    if (!editing_ || settings == nullptr || !settings->SetActiveKeyProfile(profile_id)) {
        return false;
    }
    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::create_custom_control_profile() -> bool {
    auto* const settings{input_user_settings()};
    if (!editing_ || settings == nullptr) {
        return false;
    }

    auto const source_bindings{control_bindings(EHardwareDevicePrimaryType::Unspecified)};
    if (source_bindings.IsEmpty()) {
        return false;
    }
    auto const profiles{control_profiles()};
    auto custom_number{1};
    FText display_name;
    for (;;) {
        display_name = FText::Format(NSLOCTEXT("Controls", "CustomProfileName", "Custom {0}"),
                                     FText::AsNumber(custom_number));
        if (!profiles.ContainsByPredicate([&display_name](auto const& profile) {
                return profile.display_name.EqualTo(display_name);
            })) {
            break;
        }
        ++custom_number;
    }

    FPlayerMappableKeyProfileCreationArgs arguments{};
    arguments.ProfileStringIdentifier = FString::Printf(
        TEXT("SpaceGame.Controls.Custom.%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    arguments.UserId = settings->GetLocalPlayer() != nullptr
                         ? settings->GetLocalPlayer()->GetPlatformUserId()
                         : PLATFORMUSERID_NONE;
    arguments.DisplayName = display_name;
    arguments.bSetAsCurrentProfile = false;
    auto* const profile{
        settings->create_custom_key_profile(arguments, settings->GetActiveKeyProfileId())};
    if (!IsValid(profile)) {
        return false;
    }

    for (auto const& binding : source_bindings) {
        FMapPlayerKeyArgs map_arguments{};
        map_arguments.MappingName = binding.address.mapping_name;
        map_arguments.Slot = binding.address.slot;
        map_arguments.NewKey = binding.current_key;
        map_arguments.HardwareDeviceId = binding.address.hardware_device_id;
        map_arguments.ProfileIdString = arguments.ProfileStringIdentifier;
        FGameplayTagContainer failure_reason;
        settings->MapPlayerKey(map_arguments, failure_reason);
        if (!failure_reason.IsEmpty()) {
            settings->delete_custom_key_profile(arguments.ProfileStringIdentifier);
            UE_LOG(LogTemp,
                   Warning,
                   TEXT("Could not copy control '%s' to custom profile: %s"),
                   *binding.address.mapping_name.ToString(),
                   *failure_reason.ToStringSimple());
            return false;
        }
    }
    if (!settings->SetActiveKeyProfile(arguments.ProfileStringIdentifier)) {
        settings->delete_custom_key_profile(arguments.ProfileStringIdentifier);
        return false;
    }
    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::rename_active_custom_control_profile(FString const& display_name)
    -> bool {
    auto* const settings{input_user_settings()};
    if (!editing_ || settings == nullptr ||
        !is_custom_control_profile_id(settings->GetActiveKeyProfileId())) {
        return false;
    }

    auto normalized{display_name};
    normalized.TrimStartAndEndInline();
    constexpr int32 maximum_profile_name_length{48};
    if (normalized.IsEmpty() || normalized.Len() > maximum_profile_name_length) {
        return false;
    }
    auto const profiles{control_profiles()};
    if (profiles.ContainsByPredicate([&normalized, settings](auto const& profile) {
            return profile.id != settings->GetActiveKeyProfileId() &&
                   profile.display_name.ToString().Equals(normalized, ESearchCase::IgnoreCase);
        })) {
        return false;
    }
    if (!settings->rename_custom_key_profile(settings->GetActiveKeyProfileId(),
                                             FText::FromString(normalized))) {
        return false;
    }
    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::delete_custom_control_profile(FString const& profile_id) -> bool {
    auto* const settings{input_user_settings()};
    if (!editing_ || settings == nullptr || !is_custom_control_profile_id(profile_id) ||
        !settings->delete_custom_key_profile(profile_id)) {
        return false;
    }
    if (settings->GetKeyProfileWithId(profile_id) != nullptr ||
        settings->GetActiveKeyProfileId() == profile_id) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Custom control profile '%s' remained after deletion"),
               *profile_id);
        return false;
    }
    settings_changed.Broadcast();
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
    auto const conflicts{bindings.FilterByPredicate([&address, key](auto const& binding) {
        return binding.address.profile_id == address.profile_id &&
               binding.address.hardware_device_id == address.hardware_device_id &&
               binding.address != address && binding.current_key == key;
    })};
    if (!conflicts.IsEmpty() && !replace_conflicts) {
        return false;
    }

    auto map_key = [settings](FControlBindingAddress const& target, FKey const mapped_key) {
        FMapPlayerKeyArgs arguments{};
        arguments.MappingName = target.mapping_name;
        arguments.Slot = target.slot;
        arguments.NewKey = mapped_key;
        arguments.HardwareDeviceId = target.hardware_device_id;
        arguments.ProfileIdString = target.profile_id;
        FGameplayTagContainer failure_reason;
        settings->MapPlayerKey(arguments, failure_reason);
        if (!failure_reason.IsEmpty()) {
            UE_LOG(LogTemp,
                   Warning,
                   TEXT("Could not map control '%s': %s"),
                   *target.mapping_name.ToString(),
                   *failure_reason.ToStringSimple());
            return false;
        }
        return true;
    };

    TArray<FControlBindingView> unmapped_conflicts;
    unmapped_conflicts.Reserve(conflicts.Num());
    for (auto const& conflict : conflicts) {
        if (!map_key(conflict.address, EKeys::Invalid)) {
            for (auto const& unmapped : unmapped_conflicts) {
                static_cast<void>(map_key(unmapped.address, unmapped.current_key));
            }
            return false;
        }
        unmapped_conflicts.Add(conflict);
    }
    if (!map_key(address, key)) {
        for (auto const& unmapped : unmapped_conflicts) {
            static_cast<void>(map_key(unmapped.address, unmapped.current_key));
        }
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
    if (!editing_ || settings == nullptr || is_custom_control_profile_id(address.profile_id)) {
        return false;
    }
    FMapPlayerKeyArgs arguments{};
    arguments.MappingName = address.mapping_name;
    arguments.Slot = address.slot;
    arguments.HardwareDeviceId = address.hardware_device_id;
    arguments.ProfileIdString = address.profile_id;
    FGameplayTagContainer failure_reason;
    settings->UnMapPlayerKey(arguments, failure_reason);
    if (!failure_reason.IsEmpty()) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Could not reset control '%s': %s"),
               *address.mapping_name.ToString(),
               *failure_reason.ToStringSimple());
        return false;
    }
    settings_changed.Broadcast();
    return true;
}

auto UGameSettingsSubsystem::reset_active_control_profile() -> bool {
    auto* const settings{input_user_settings()};
    if (!editing_ || settings == nullptr ||
        is_custom_control_profile_id(settings->GetActiveKeyProfileId())) {
        return false;
    }
    FGameplayTagContainer failure_reason;
    settings->ResetKeyProfileIdToDefault(settings->GetActiveKeyProfileId(), failure_reason);
    if (!failure_reason.IsEmpty()) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Could not reset active control profile: %s"),
               *failure_reason.ToStringSimple());
        return false;
    }
    settings_changed.Broadcast();
    return true;
}

void UGameSettingsSubsystem::capture_input_edit_state() {
    auto const* const settings{input_user_settings()};
    if (settings == nullptr) {
        applied_control_profiles_.Reset();
        applied_control_bindings_.Reset();
        applied_control_profile_id_.Reset();
        return;
    }
    applied_control_profiles_ = control_profiles();
    applied_control_bindings_ = all_control_bindings();
    applied_control_profile_id_ = settings->GetActiveKeyProfileId();
}

void UGameSettingsSubsystem::restore_input_edit_state() {
    auto* const settings{input_user_settings()};
    if (settings == nullptr || applied_control_profile_id_.IsEmpty()) {
        return;
    }

    auto const current_profiles{control_profiles()};
    for (auto const& profile : current_profiles) {
        if (profile.custom &&
            !applied_control_profiles_.ContainsByPredicate(
                [&profile](auto const& applied) { return applied.id == profile.id; })) {
            settings->delete_custom_key_profile(profile.id);
        }
    }
    for (auto const& profile : applied_control_profiles_) {
        if (!profile.custom) {
            continue;
        }
        if (settings->GetKeyProfileWithId(profile.id) != nullptr) {
            settings->rename_custom_key_profile(profile.id, profile.display_name);
            continue;
        }
        FPlayerMappableKeyProfileCreationArgs arguments{};
        arguments.ProfileStringIdentifier = profile.id;
        arguments.UserId = settings->GetLocalPlayer() != nullptr
                             ? settings->GetLocalPlayer()->GetPlatformUserId()
                             : PLATFORMUSERID_NONE;
        arguments.DisplayName = profile.display_name;
        arguments.bSetAsCurrentProfile = false;
        settings->create_custom_key_profile(arguments, profile.mapping_profile_id);
    }

    for (auto const& profile_pair : settings->GetAllAvailableKeyProfiles()) {
        FGameplayTagContainer failure_reason;
        settings->ResetKeyProfileIdToDefault(profile_pair.Key, failure_reason);
    }
    for (auto const& binding : applied_control_bindings_) {
        if (binding.current_key == binding.default_key) {
            continue;
        }
        FMapPlayerKeyArgs arguments{};
        arguments.MappingName = binding.address.mapping_name;
        arguments.Slot = binding.address.slot;
        arguments.NewKey = binding.current_key;
        arguments.HardwareDeviceId = binding.address.hardware_device_id;
        arguments.ProfileIdString = binding.address.profile_id;
        FGameplayTagContainer failure_reason;
        settings->MapPlayerKey(arguments, failure_reason);
    }
    settings->SetActiveKeyProfile(applied_control_profile_id_);
    settings->ApplySettings();
}

auto UGameSettingsSubsystem::input_is_dirty() const -> bool {
    auto const* const settings{input_user_settings()};
    if (settings == nullptr) {
        return false;
    }
    if (settings->GetActiveKeyProfileId() != applied_control_profile_id_) {
        return true;
    }
    auto const profiles{control_profiles()};
    if (profiles.Num() != applied_control_profiles_.Num()) {
        return true;
    }
    auto const profile_count{profiles.Num()};
    for (int32 index{}; index < profile_count; ++index) {
        if (profiles[index].id != applied_control_profiles_[index].id ||
            profiles[index].mapping_profile_id !=
                applied_control_profiles_[index].mapping_profile_id ||
            !profiles[index].display_name.EqualTo(applied_control_profiles_[index].display_name)) {
            return true;
        }
    }
    auto const current{all_control_bindings()};
    if (current.Num() != applied_control_bindings_.Num()) {
        return true;
    }
    auto const binding_count{current.Num()};
    for (int32 index{}; index < binding_count; ++index) {
        if (current[index].address != applied_control_bindings_[index].address ||
            current[index].current_key != applied_control_bindings_[index].current_key) {
            return true;
        }
    }
    return false;
}

} // namespace ml::ioj
