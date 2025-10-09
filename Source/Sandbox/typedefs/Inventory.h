#pragma once

#include "CoreMinimal.h"

#include "Sandbox/generated/strong_typedefs/Coord.h"
#include "Sandbox/generated/strong_typedefs/Dimensions.h"
#include "Sandbox/generated/strong_typedefs/StackSize.h"

class IInventoryItem;

namespace ml::inventory {
using ItemPtr = TScriptInterface<IInventoryItem>;
}
