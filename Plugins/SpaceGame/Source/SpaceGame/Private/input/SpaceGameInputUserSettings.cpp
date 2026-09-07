#include "SpaceGame/input/SpaceGameInputUserSettings.h"

#include "EnhancedActionKeyMapping.h"
#include "SpaceGame/input/ControlProfiles.h"

namespace ml::ioj {

void USpaceGameKeyProfile::set_mapping_profile_id(FString const& profile_id) {
    ProfileIdentifierString = profile_id;
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

    profile->set_mapping_profile_id(source_profile->GetProfileIdString());
    for (auto const& mapping_context : RegisteredMappingContexts) {
        RegisterKeyMappingsToProfile(*profile, mapping_context);
    }
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

auto USpaceGameInputUserSettings::delete_custom_key_profile(FString const& profile_id) -> bool {
    if (!is_custom_control_profile_id(profile_id) || !SavedKeyProfilesMap.Contains(profile_id)) {
        return false;
    }
    if (GetActiveKeyProfileId() == profile_id && !SetKeyProfileToDefault()) {
        return false;
    }
    SavedKeyProfilesMap.Remove(profile_id);
    OnSettingsChanged.Broadcast(this);
    return true;
}

auto USpaceGameInputUserSettings::DetermineHardwareDeviceForActionMapping(
    FEnhancedActionKeyMapping const& action_mapping,
    UInputMappingContext const* const mapping_context) const -> FHardwareDeviceIdentifier {
    static_cast<void>(mapping_context);
    return action_mapping.Key.IsGamepadKey() ? FHardwareDeviceIdentifier::DefaultGamepad
                                             : FHardwareDeviceIdentifier::DefaultKeyboardAndMouse;
}

} // namespace ml::ioj
