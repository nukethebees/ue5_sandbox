#pragma once

#include "CoreMinimal.h"

#include "MyPlayerControllerInputActions.generated.h"

class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct FMyPlayerControllerInputActions {
    GENERATED_BODY()

    FMyPlayerControllerInputActions() = default;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* base_context{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* direct_mode_context{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* cursor_mode_context{nullptr};

    // Movement
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* warp_to_cursor{nullptr};

    // Vision
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* look{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* toggle_mouse{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* mouse_click{nullptr};

    // Weapons
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
    UInputAction* attack{nullptr};

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* interact{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* toggle_in_game_menu{nullptr};
};
