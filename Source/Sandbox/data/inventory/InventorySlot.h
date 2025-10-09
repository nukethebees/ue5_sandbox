#pragma once

#include "CoreMinimal.h"

#include "Sandbox/typedefs/Inventory.h"

struct FInventorySlot {
    using ItemPtr = ml::inventory::ItemPtr;
    using Dimensions = ml::inventory::Dimensions;

    FInventorySlot() = default;

    ItemPtr item{nullptr};
    FStackSize stack_size{};
    Dimensions dimensions{};
};
