#pragma once

#include "CoreMinimal.h"

#include "SpaceShipControllerInputs.generated.h"

class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct SPACEGAME_API FSpaceShipControllerInputs {
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Input|Flight Models")
    UInputMappingContext* starfox{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Flight Models")
    UInputMappingContext* fighter{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Flight Models")
    UInputMappingContext* skater{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Flight Models")
    UInputMappingContext* gunship{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "Input|Translation")
    UInputAction* translate_forward{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Translation")
    UInputAction* translate_right{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Translation")
    UInputAction* translate_up{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Rotation")
    UInputAction* pitch{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Rotation")
    UInputAction* yaw{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Rotation")
    UInputAction* roll{nullptr};
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
    UPROPERTY(EditDefaultsOnly, Category = "Input|Flight Models")
    UInputAction* select_starfox{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Flight Models")
    UInputAction* select_fighter{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Flight Models")
    UInputAction* select_skater{nullptr};
    UPROPERTY(EditDefaultsOnly, Category = "Input|Flight Models")
    UInputAction* select_gunship{nullptr};
};

USTRUCT(BlueprintType)
struct FGlobalControlInputs {
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    UInputMappingContext* mapping_context{nullptr};

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    UInputAction* toggle_menu{nullptr};
};
