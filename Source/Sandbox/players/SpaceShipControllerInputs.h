#pragma once

#include "CoreMinimal.h"

#include "SpaceShipControllerInputs.generated.h"

class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct FSpaceShipControllerInputs {
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    UInputMappingContext* base_context{nullptr};

    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* turn{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* fire_laser{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* fire_bomb{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* boost{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* brake{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* roll{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* barrel_roll{nullptr};

    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* cycle_next_fire_rate{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* cycle_prev_fire_rate{nullptr};
};
