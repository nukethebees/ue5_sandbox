#pragma once

#include "CoreMinimal.h"

#include "Sandbox/typedefs/Inventory.h"

struct FInventorySlot {
    using ItemPtr = ml::inventory::ItemPtr;
    using StackSize = ml::inventory::StackSize;
    using Dimensions = ml::inventory::Dimensions;

    FInventorySlot() = default;

    ItemPtr item{nullptr};
    StackSize stack_size{};
    Dimensions dimensions{};
};
