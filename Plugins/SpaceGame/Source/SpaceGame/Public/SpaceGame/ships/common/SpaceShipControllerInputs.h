#pragma once

#include "CoreMinimal.h"

#include "SpaceShipControllerInputs.generated.h"

class UInputMappingContext;
class UInputAction;

namespace ioj {
USTRUCT(BlueprintType)
struct SPACEGAME_API FSpaceShipControllerInputs {
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    UInputMappingContext* starfox{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "Input|Rotation")
    UInputAction* pitch{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Rotation")
    UInputAction* yaw{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Propulsion")
    UInputAction* accelerate{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Propulsion")
    UInputAction* brake{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Propulsion")
    UInputAction* boost{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Propulsion")
    UInputAction* emergency_brake{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Weapons")
    UInputAction* fire_primary{nullptr};
};

USTRUCT(BlueprintType)
struct FGlobalControlInputs {
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    UInputMappingContext* mapping_context{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    UInputAction* toggle_menu{nullptr};
};

} // namespace ioj
