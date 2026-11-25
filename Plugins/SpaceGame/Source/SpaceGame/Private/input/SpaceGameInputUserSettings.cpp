#include "SpaceGame/input/SpaceGameInputUserSettings.h"

#include "ioj/input/response.h"
#include "SpaceGame/input/CanonicalShipControls.h"
#include "SpaceGame/input/ControlBindingMetadata.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedActionKeyMapping.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerMappableKeySettings.h"

namespace ioj {

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

bool USpaceGameInputUserSettings::save_profile() {
    auto* const player{GetLocalPlayer()};
    return IsValid(player) && UGameplayStatics::SaveGameToSlot(
                                  this, GetSaveFilename(player), player->GetLocalPlayerIndex());
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

} // namespace ioj
