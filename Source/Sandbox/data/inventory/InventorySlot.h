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
                   FDimensions item_dimensions,
                   FCoord origin)
        : item(item)
        , stack_size(stack_size)
        , item_dimensions(item_dimensions)
        , origin(origin) {}
    FInventorySlot(TScriptInterface<IInventoryItem> item)
        : item(item)
        , stack_size(1)
        , item_dimensions(item->get_size()) {}

    auto area() const { return item_dimensions.area(); }
    auto width() const { return is_rotated ? item_dimensions.y() : item_dimensions.x(); }
    auto height() const { return is_rotated ? item_dimensions.x() : item_dimensions.y(); }
    auto dimensions() const {
        if (is_rotated) {
            return FDimensions{item_dimensions.y(), item_dimensions.x()};
        }
        return item_dimensions;
    }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TScriptInterface<IInventoryItem> item{nullptr};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FStackSize stack_size{1};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FDimensions item_dimensions{1, 1};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FCoord origin{0, 0};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool is_rotated{false};
};
