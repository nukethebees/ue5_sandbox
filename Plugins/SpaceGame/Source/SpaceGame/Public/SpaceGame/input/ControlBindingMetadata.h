#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "ControlBindingMetadata.generated.h"

namespace ml::ioj {

UENUM()
enum class EControlBindingGroup : uint8 {
    Flight,
    Combat,
    Utility,
};

UCLASS()
class SPACEGAME_API UControlBindingMetadata final : public UObject {
    GENERATED_BODY()
  public:
    UPROPERTY()
    EControlBindingGroup group{EControlBindingGroup::Flight};

    UPROPERTY()
    int32 display_order{};
};

SPACEGAME_API auto control_binding_group_label(EControlBindingGroup group) -> FText;

} // namespace ml::ioj
