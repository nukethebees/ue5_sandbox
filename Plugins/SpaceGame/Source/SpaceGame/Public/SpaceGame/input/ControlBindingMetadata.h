#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "ControlBindingMetadata.generated.h"

namespace ioj {

UENUM()
enum class EControlBindingGroup : uint8 {
    Flight,
    Combat,
    Utility,
};

SPACEGAME_API auto control_binding_group_label(EControlBindingGroup group) -> FText;

} // namespace ioj
