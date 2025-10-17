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
                   FDimensions dimensions,
                   FCoord origin)
        : item(item)
        , stack_size(stack_size)
        , dimensions(dimensions)
        , origin(origin) {}
    FInventorySlot(TScriptInterface<IInventoryItem> item)
        : item(item)
        , stack_size(1)
        , dimensions(item->get_size()) {}

    auto area() const { return dimensions.area(); }
    auto width() const { return is_rotated ? dimensions.y() : dimensions.x(); }
    auto height() const { return is_rotated ? dimensions.x() : dimensions.y(); }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TScriptInterface<IInventoryItem> item{nullptr};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FStackSize stack_size{1};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FDimensions dimensions{1, 1};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FCoord origin{0, 0};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool is_rotated{false};
};
