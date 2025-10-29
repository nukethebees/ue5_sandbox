#pragma once

#include "CoreMinimal.h"

#include "MyPlayerControllerInputActions.generated.h"

class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct FMyPlayerControllerInputActions {
    GENERATED_BODY()

    FMyPlayerControllerInputActions() = default;

    // Contexts
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* base_context{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* direct_mode_context{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* cursor_mode_context{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* character_context{nullptr};

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* move{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* jump{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* crouch{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* sprint{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* warp_to_cursor{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* jetpack{nullptr};

    // Vision
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* look{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* toggle_mouse{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* mouse_click{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* cycle_camera{nullptr};

    // Combat
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    UInputAction* attack{nullptr};

    // Torch
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* toggle_torch{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* scroll_torch_cone{nullptr};

    // Inventory
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    UInputAction* cycle_next_weapon{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    UInputAction* cycle_prev_weapon{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    UInputAction* unequip_weapon{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    UInputAction* drop_weapon{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    UInputAction* reload_weapon{nullptr};

    // Interaction
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* interact{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* drop_waypoint{nullptr};

    // UI
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* toggle_in_game_menu{nullptr};
};
