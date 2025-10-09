#pragma once

#include "CoreMinimal.h"

#include "Sandbox/typedefs/Inventory.h"

struct FInventorySlot {
    using ItemPtr = ml::inventory::ItemPtr;

    FInventorySlot() = default;

    ItemPtr item{nullptr};
    UPROPERTY()
    FStackSize stack_size{};
    UPROPERTY()
    FDimensions dimensions{};
};
