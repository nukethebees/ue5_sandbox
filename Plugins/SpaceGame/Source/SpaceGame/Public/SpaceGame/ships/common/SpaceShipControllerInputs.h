#pragma once

#include "CoreMinimal.h"

#include "SpaceShipControllerInputs.generated.h"

class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct FSpaceShipControllerInputs {
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TArray<UInputMappingContext*> mapping_contexts;

    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* move{nullptr};
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
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* cycle_input_mapping_context{nullptr};

    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* lateral_move{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* vertical_move{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* sample_and_hold{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* ship_2d_control{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* ship_1d_control_x{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* ship_1d_control_y{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* cycle_next_control_mode{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* cycle_previous_control_mode{nullptr};
    UPROPERTY(EditAnywhere, Category = "Input")
    UInputAction* pause_game{nullptr};
};
