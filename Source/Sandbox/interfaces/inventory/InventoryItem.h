#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Sandbox/typedefs/Inventory.h"

#include "InventoryItem.generated.h"

UINTERFACE(MinimalAPI)
class UInventoryItem : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API IInventoryItem {
    GENERATED_BODY()
  public:
    using Ptr = TScriptInterface<IInventoryItem>;
};
