#pragma once

#include "CoreMinimal.h"

#include "SpaceShipControllerInputs.generated.h"

class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct FSpaceShipControllerInputs {
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* base_context{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* turn{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* fire_laser{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* fire_bomb{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* boost{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* brake{nullptr};
};
