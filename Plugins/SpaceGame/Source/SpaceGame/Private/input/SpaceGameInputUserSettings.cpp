#include "SpaceGame/input/SpaceGameInputUserSettings.h"

#include "SpaceGame/input/CanonicalShipControls.h"
#include "SpaceGame/input/ControlBindingMetadata.h"

#include "EnhancedActionKeyMapping.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"

namespace ml::ioj {

void USpaceGameInputUserSettings::finalize_canonical_registration() {
    auto* const profile{GetDefaultKeyProfile()};
    if (!IsValid(profile)) {
        UE_LOG(LogTemp, Error, TEXT("Canonical input registration has no default key profile"));
        return;
    }

    static_cast<void>(SetKeyProfileToDefault());

    TArray<FString> obsolete_ids;
    for (auto const& pair : SavedKeyProfilesMap) {
        if (pair.Value != profile) {
            obsolete_ids.Add(pair.Key);
        }
    }
    for (auto const& id : obsolete_ids) {
        SavedKeyProfilesMap.Remove(id);
    }
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
void USpaceGameInputUserSettings::set_invert_mouse_yaw(bool const value) noexcept {
    invert_mouse_yaw_ = value;
}

void USpaceGameInputUserSettings::set_invert_gamepad_pitch(bool const value) noexcept {
    invert_gamepad_pitch_ = value;
}
void USpaceGameInputUserSettings::set_invert_gamepad_yaw(bool const value) noexcept {
    invert_gamepad_yaw_ = value;
}
void USpaceGameInputUserSettings::set_invert_gamepad_roll(bool const value) noexcept {
    invert_gamepad_roll_ = value;
}
void USpaceGameInputUserSettings::set_invert_gamepad_vertical_translation(
    bool const value) noexcept {
    invert_gamepad_vertical_translation_ = value;
}

auto USpaceGameInputUserSettings::chord_mapping_for_mapping(FString const& profile_id,
                                                            FPlayerKeyMapping const& mapping) const
    -> FPlayerKeyMapping const* {
    static_cast<void>(profile_id);
    static_cast<void>(mapping);
    return nullptr;
}

auto USpaceGameInputUserSettings::control_binding_metadata(FString const& profile_id,
                                                           FPlayerKeyMapping const& mapping) const
    -> UControlBindingMetadata const* {
    auto const* const profile{GetKeyProfileWithId(profile_id)};
    if (!IsValid(profile)) {
        return nullptr;
    }

    auto const gamepad{mapping.GetPrimaryDeviceType() == EHardwareDevicePrimaryType::Gamepad};
    for (auto const& definition : canonical_ship_control_contexts()) {
        auto const* const context{load_ship_control_context(definition.scope)};
        if (!IsValid(context) || !IsMappingContextRegistered(context)) {
            continue;
        }
        for (auto const& source : context->GetMappingsForProfile(profile->GetProfileIdString())) {
            if (source.GetMappingName() != mapping.GetMappingName() ||
                source.Key.IsGamepadKey() != gamepad) {
                continue;
            }
            auto const* const settings{source.GetPlayerMappableKeySettings()};
            return IsValid(settings) ? Cast<UControlBindingMetadata>(settings->Metadata) : nullptr;
        }
    }
    return nullptr;
}

auto USpaceGameInputUserSettings::DetermineHardwareDeviceForActionMapping(
    FEnhancedActionKeyMapping const& action_mapping,
    UInputMappingContext const* const mapping_context) const -> FHardwareDeviceIdentifier {
    static_cast<void>(mapping_context);
    return action_mapping.Key.IsGamepadKey() ? FHardwareDeviceIdentifier::DefaultGamepad
                                             : FHardwareDeviceIdentifier::DefaultKeyboardAndMouse;
}

} // namespace ml::ioj
