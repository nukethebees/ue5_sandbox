#pragma once

#include "CoreMinimal.h"

#include "Sandbox/inventory/Coord.h"
#include "Sandbox/inventory/StackSize.h"
#include "Sandbox/inventory/InventoryItem.h"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "InventoryEntry.generated.h"

USTRUCT(BlueprintType)
struct FInventoryEntry {
    GENERATED_BODY()

    FInventoryEntry() = default;
    FInventoryEntry(TScriptInterface<IInventoryItem> item,
                    FStackSize stack_size,
                    FDimensions item_dimensions,
                    FCoord origin)
        : item(item)
        , stack_size(stack_size)
        , origin(origin)
        , item_dimensions(item_dimensions) {}
    FInventoryEntry(TScriptInterface<IInventoryItem> item)
        : item(item)
        , stack_size(1)
        , item_dimensions(item->get_size()) {}

    auto area() const { return item_dimensions.area(); }
    auto width() const { return is_rotated ? item_dimensions.y() : item_dimensions.x(); }
    auto height() const { return is_rotated ? item_dimensions.x() : item_dimensions.y(); }
    auto dimensions() const { return is_rotated ? item_dimensions.rotated() : item_dimensions; }
    auto const& name() const {
        check(item);
        return item->get_name();
    }
    auto aspect_ratio() const {
        check(item);
        return dimensions().aspect_ratio();
    }
    auto item_type() const {
        check(item);
        return item->get_item_type();
    }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TScriptInterface<IInventoryItem> item{nullptr};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FStackSize stack_size{1};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FCoord origin{0, 0};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    bool is_rotated{false};
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FDimensions item_dimensions{1, 1};
};
