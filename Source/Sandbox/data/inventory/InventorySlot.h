#pragma once

#include "CoreMinimal.h"

#include "Sandbox/interfaces/inventory/InventoryItem.h"
#include "Sandbox/SandboxLogCategories.h"
#include "Sandbox/typedefs/Inventory.h"

#include "InventorySlot.generated.h"

USTRUCT(BlueprintType)
struct FInventorySlot {
    GENERATED_BODY()

    FInventorySlot() = default;
    FInventorySlot(TScriptInterface<IInventoryItem> item,
                   FStackSize stack_size,
                   FDimensions dimensions)
        : item(item)
        , stack_size(stack_size)
        , dimensions(dimensions) {}
    FInventorySlot(TScriptInterface<IInventoryItem> item)
        : item(item)
        , stack_size(1)
        , dimensions(item->get_size()) {}

    UPROPERTY()
    TScriptInterface<IInventoryItem> item{nullptr};
    UPROPERTY()
    FStackSize stack_size{1};
    UPROPERTY()
    FDimensions dimensions{};
};
