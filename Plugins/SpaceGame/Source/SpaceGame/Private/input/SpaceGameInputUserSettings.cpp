#include "SpaceGame/input/SpaceGameInputUserSettings.h"

#include "EnhancedActionKeyMapping.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "SpaceGame/input/ControlProfiles.h"

namespace ml::ioj {

void USpaceGameKeyProfile::initialize_from(UEnhancedPlayerMappableKeyProfile const& source) {
    PlayerMappedKeys = source.GetPlayerMappingRows();
}

void USpaceGameKeyProfile::set_runtime_profile_id(FString const& profile_id) {
    ProfileIdentifierString = profile_id;
}

void USpaceGameInputUserSettings::Initialize(ULocalPlayer* const local_player) {
    Super::Initialize(local_player);
    migrate_custom_profile_metadata();
    apply_active_mapping_profile_id();
}

auto USpaceGameInputUserSettings::SetActiveKeyProfile(FString const& profile_id) -> bool {
    restore_active_profile_id();
    auto const changed{Super::SetActiveKeyProfile(profile_id)};
    apply_active_mapping_profile_id();
    return changed;
}

void USpaceGameInputUserSettings::set_mouse_turn_sensitivity(float const value) noexcept {
    mouse_turn_sensitivity_ = FMath::Max(0.0f, value);
}

void USpaceGameInputUserSettings::set_gamepad_turn_sensitivity(float const value) noexcept {
    gamepad_turn_sensitivity_ = FMath::Max(0.0f, value);
}

void USpaceGameInputUserSettings::set_gamepad_turn_dead_zone(float const value) noexcept {
    gamepad_turn_dead_zone_ = FMath::Clamp(value, 0.0f, 0.95f);
}

void USpaceGameInputUserSettings::set_gamepad_move_dead_zone(float const value) noexcept {
    gamepad_move_dead_zone_ = FMath::Clamp(value, 0.0f, 0.95f);
}

void USpaceGameInputUserSettings::set_invert_mouse_pitch(bool const value) noexcept {
    invert_mouse_pitch_ = value;
}

void USpaceGameInputUserSettings::set_invert_gamepad_pitch(bool const value) noexcept {
    invert_gamepad_pitch_ = value;
}

auto USpaceGameInputUserSettings::create_custom_key_profile(
    FPlayerMappableKeyProfileCreationArgs const& arguments, FString const& source_profile_id)
    -> UEnhancedPlayerMappableKeyProfile* {
    auto const* const source_profile{GetKeyProfileWithId(source_profile_id)};
    if (!is_custom_control_profile_id(arguments.ProfileStringIdentifier) ||
        !IsValid(source_profile)) {
        return nullptr;
    }

    auto custom_arguments{arguments};
    custom_arguments.ProfileType = USpaceGameKeyProfile::StaticClass();
    auto* const profile{Cast<USpaceGameKeyProfile>(CreateNewKeyProfile(custom_arguments))};
    if (!IsValid(profile)) {
        return nullptr;
    }

    auto const mapping_profile_id{custom_key_profile_source_id(source_profile_id)};
    custom_profile_source_ids_.Add(arguments.ProfileStringIdentifier,
                                   mapping_profile_id.IsEmpty() ? source_profile_id
                                                                : mapping_profile_id);
    custom_profile_names_.Add(arguments.ProfileStringIdentifier, arguments.DisplayName.ToString());
    profile->initialize_from(*source_profile);
    if (profile->GetPlayerMappingRows().IsEmpty()) {
        delete_custom_key_profile(arguments.ProfileStringIdentifier);
        UE_LOG(LogTemp,
               Warning,
               TEXT("Could not populate custom control profile '%s' from '%s'"),
               *arguments.ProfileStringIdentifier,
               *source_profile_id);
        return nullptr;
    }
    return profile;
}

auto USpaceGameInputUserSettings::rename_custom_key_profile(FString const& profile_id,
                                                            FText const& display_name) -> bool {
    auto* const profile{GetKeyProfileWithId(profile_id)};
    if (!is_custom_control_profile_id(profile_id) || !IsValid(profile)) {
        return false;
    }
    profile->SetDisplayName(display_name);
    custom_profile_names_.Add(profile_id, display_name.ToString());
    OnSettingsChanged.Broadcast(this);
    return true;
}

auto USpaceGameInputUserSettings::delete_custom_key_profile(FString const& profile_id) -> bool {
    if (!is_custom_control_profile_id(profile_id) || !SavedKeyProfilesMap.Contains(profile_id)) {
        return false;
    }
    if (GetActiveKeyProfileId() == profile_id &&
        !SetActiveKeyProfile(control_profile_definitions()[0].id)) {
        return false;
    }
    if (SavedKeyProfilesMap.Remove(profile_id) != 1 || GetActiveKeyProfileId() == profile_id) {
        return false;
    }
    custom_profile_source_ids_.Remove(profile_id);
    custom_profile_names_.Remove(profile_id);
    OnSettingsChanged.Broadcast(this);
    return true;
}

auto USpaceGameInputUserSettings::custom_key_profile_source_id(FString const& profile_id) const
    -> FString {
    if (auto const* const source_id{custom_profile_source_ids_.Find(profile_id)}) {
        return *source_id;
    }
    return {};
}

auto USpaceGameInputUserSettings::custom_key_profile_display_name(FString const& profile_id) const
    -> FText {
    if (auto const* const display_name{custom_profile_names_.Find(profile_id)}) {
        return FText::FromString(*display_name);
    }
    auto const* const profile{GetKeyProfileWithId(profile_id)};
    return IsValid(profile) ? profile->GetProfileDisplayName() : FText::GetEmpty();
}

auto USpaceGameInputUserSettings::chord_mapping_for_mapping(FString const& profile_id,
                                                            FPlayerKeyMapping const& mapping) const
    -> FPlayerKeyMapping const* {
    auto const* const profile{GetKeyProfileWithId(profile_id)};
    auto const* const action{mapping.GetAssociatedInputAction()};
    if (!IsValid(profile) || !IsValid(action)) {
        return {};
    }

    auto source_profile_id{custom_key_profile_source_id(profile_id)};
    if (source_profile_id.IsEmpty()) {
        source_profile_id = profile->GetProfileIdString();
    }
    for (auto const& mapping_context : RegisteredMappingContexts) {
        auto const* const context{mapping_context.Get()};
        if (!IsValid(context)) {
            continue;
        }
        for (auto const& source_mapping : context->GetMappingsForProfile(source_profile_id)) {
            if (source_mapping.Action != action || source_mapping.Key != mapping.GetDefaultKey()) {
                continue;
            }
            for (auto const& trigger : source_mapping.Triggers) {
                auto const* const chord_trigger{Cast<UInputTriggerChordAction>(trigger)};
                if (!IsValid(chord_trigger) || !IsValid(chord_trigger->ChordAction)) {
                    continue;
                }
                for (auto const& row : profile->GetPlayerMappingRows()) {
                    for (auto const& candidate : row.Value.Mappings) {
                        if (candidate.GetAssociatedInputAction() == chord_trigger->ChordAction &&
                            candidate.GetPrimaryDeviceType() == mapping.GetPrimaryDeviceType()) {
                            return &candidate;
                        }
                    }
                }
                return nullptr;
            }
        }
    }
    return {};
}

auto USpaceGameInputUserSettings::RegisterKeyMappingsToProfile(
    UEnhancedPlayerMappableKeyProfile& profile, UInputMappingContext const* const mapping_context)
    -> bool {
    auto profile_id{profile.GetProfileIdString()};
    for (auto const& profile_pair : SavedKeyProfilesMap) {
        if (profile_pair.Value == &profile) {
            profile_id = profile_pair.Key;
            break;
        }
    }
    auto const source_profile_id{custom_key_profile_source_id(profile_id)};
    auto* const custom_profile{Cast<USpaceGameKeyProfile>(&profile)};
    auto const mapping_profile_id{source_profile_id.IsEmpty() ? profile.GetProfileIdString()
                                                              : source_profile_id};
    if (custom_profile == nullptr || source_profile_id.IsEmpty()) {
        auto const result{Super::RegisterKeyMappingsToProfile(profile, mapping_context)};
        if (result) {
            prune_stale_mapping_rows(profile, mapping_profile_id);
        }
        return result;
    }

    auto const runtime_profile_id{profile.GetProfileIdString()};
    custom_profile->set_runtime_profile_id(source_profile_id);
    auto const result{Super::RegisterKeyMappingsToProfile(profile, mapping_context)};
    custom_profile->set_runtime_profile_id(runtime_profile_id);
    if (result) {
        prune_stale_mapping_rows(profile, mapping_profile_id);
    }
    return result;
}

auto USpaceGameInputUserSettings::DetermineHardwareDeviceForActionMapping(
    FEnhancedActionKeyMapping const& action_mapping,
    UInputMappingContext const* const mapping_context) const -> FHardwareDeviceIdentifier {
    static_cast<void>(mapping_context);
    return action_mapping.Key.IsGamepadKey() ? FHardwareDeviceIdentifier::DefaultGamepad
                                             : FHardwareDeviceIdentifier::DefaultKeyboardAndMouse;
}

void USpaceGameInputUserSettings::apply_active_mapping_profile_id() {
    auto* const profile{Cast<USpaceGameKeyProfile>(GetActiveKeyProfile())};
    auto const source_profile_id{custom_key_profile_source_id(GetActiveKeyProfileId())};
    if (profile != nullptr && !source_profile_id.IsEmpty()) {
        profile->set_runtime_profile_id(source_profile_id);
    }
}

void USpaceGameInputUserSettings::migrate_custom_profile_metadata() {
    for (auto const& profile_pair : SavedKeyProfilesMap) {
        if (!is_custom_control_profile_id(profile_pair.Key) || !IsValid(profile_pair.Value)) {
            continue;
        }
        if (!custom_profile_source_ids_.Contains(profile_pair.Key)) {
            auto source_profile_id{profile_pair.Value->GetProfileIdString()};
            if (!control_profile_definitions().ContainsByPredicate(
                    [&source_profile_id](auto const& definition) {
                        return definition.id == source_profile_id;
                    })) {
                source_profile_id = control_profile_definitions()[0].id;
            }
            custom_profile_source_ids_.Add(profile_pair.Key, MoveTemp(source_profile_id));
        }
        if (!custom_profile_names_.Contains(profile_pair.Key) &&
            !profile_pair.Value->GetProfileDisplayName().IsEmpty()) {
            custom_profile_names_.Add(profile_pair.Key,
                                      profile_pair.Value->GetProfileDisplayName().ToString());
        }
    }
}

void USpaceGameInputUserSettings::prune_stale_mapping_rows(
    UEnhancedPlayerMappableKeyProfile& profile, FString const& mapping_profile_id) const {
    TSet<FName> registered_mapping_names;
    for (auto const& mapping_context : RegisteredMappingContexts) {
        if (!IsValid(mapping_context)) {
            continue;
        }
        for (auto const& mapping : mapping_context->GetMappingsForProfile(mapping_profile_id)) {
            if (mapping.IsPlayerMappable()) {
                registered_mapping_names.Add(mapping.GetMappingName());
            }
        }
    }

    auto& rows{const_cast<TMap<FName, FKeyMappingRow>&>(profile.GetPlayerMappingRows())};
    for (auto iterator{rows.CreateIterator()}; iterator; ++iterator) {
        if (!registered_mapping_names.Contains(iterator.Key())) {
            iterator.RemoveCurrent();
        }
    }
}

void USpaceGameInputUserSettings::restore_active_profile_id() {
    auto* const profile{Cast<USpaceGameKeyProfile>(GetActiveKeyProfile())};
    if (profile != nullptr) {
        profile->set_runtime_profile_id(GetActiveKeyProfileId());
    }
}

} // namespace ml::ioj
