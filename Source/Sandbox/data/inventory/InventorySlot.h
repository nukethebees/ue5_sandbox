#pragma once

#include "CoreMinimal.h"

#include "Sandbox/typedefs/Inventory.h"

#include "InventorySlot.generated.h"

USTRUCT(BlueprintType)
struct FInventorySlot {
    GENERATED_BODY()

    FInventorySlot() = default;

    UPROPERTY()
    FItemPtr item{nullptr};
    UPROPERTY()
    FStackSize stack_size{};
    UPROPERTY()
    FDimensions dimensions{};
};
