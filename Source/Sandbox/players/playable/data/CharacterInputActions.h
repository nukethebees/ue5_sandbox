#pragma once

#include "CoreMinimal.h"

#include "CharacterInputActions.generated.h"

class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct FCharacterInputActions {
    GENERATED_BODY()

    FCharacterInputActions() = default;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* character_context{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* move{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* jump{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* crouch{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* sprint{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* jetpack{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* cycle_camera{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* toggle_torch{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* scroll_torch_cone{nullptr};
};
