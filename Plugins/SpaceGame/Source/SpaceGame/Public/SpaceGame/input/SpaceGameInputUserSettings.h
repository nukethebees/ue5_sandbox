#pragma once

#include "UserSettings/EnhancedInputUserSettings.h"

#include "SpaceGameInputUserSettings.generated.h"

namespace ioj {

class UControlBindingMetadata;

UCLASS()
class SPACEGAME_API USpaceGameInputUserSettings final : public UEnhancedInputUserSettings {
    GENERATED_BODY()
  public:
    void finalize_canonical_registration();
    [[nodiscard]] bool save_profile();

    [[nodiscard]] auto chord_mapping_for_mapping(FString const& profile_id,
                                                 FPlayerKeyMapping const& mapping) const
        -> FPlayerKeyMapping const*;
    [[nodiscard]] auto control_binding_metadata(FString const& profile_id,
                                                FPlayerKeyMapping const& mapping) const
        -> UControlBindingMetadata const*;
  protected:
    auto DetermineHardwareDeviceForActionMapping(FEnhancedActionKeyMapping const& action_mapping,
                                                 UInputMappingContext const* mapping_context) const
        -> FHardwareDeviceIdentifier override;
};

} // namespace ioj
