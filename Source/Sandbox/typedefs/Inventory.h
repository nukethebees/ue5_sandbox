#pragma once

#include "CoreMinimal.h"

class IInventoryItem;

namespace ml::inventory {
using StackSize = uint32;
using Coord = FIntPoint;
using Dimensions = FIntPoint;
using ItemPtr = TScriptInterface<IInventoryItem>;
}
