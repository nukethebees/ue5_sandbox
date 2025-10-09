#pragma once

#include "CoreMinimal.h"

#include "Sandbox/generated/strong_typedefs/StackSize.h"

class IInventoryItem;

namespace ml::inventory {
using Coord = FIntPoint;
using Dimensions = FIntPoint;
using ItemPtr = TScriptInterface<IInventoryItem>;
}
