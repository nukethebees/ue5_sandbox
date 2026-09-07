#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

class UEnhancedInputUserSettings;
class UInputMappingContext;

namespace ml::ioj {

struct FControlProfileDefinition {
    FString id;
    FText display_name;
};

SPACEGAME_API auto control_profile_definitions() -> TConstArrayView<FControlProfileDefinition>;
SPACEGAME_API auto register_control_profiles(UEnhancedInputUserSettings& settings,
                                             UInputMappingContext& mapping_context) -> bool;
SPACEGAME_API auto cycle_control_profile(UEnhancedInputUserSettings& settings) -> bool;

} // namespace ml::ioj
