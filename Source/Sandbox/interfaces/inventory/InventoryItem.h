#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Sandbox/generated/strong_typedefs/Dimensions.h"

#include "InventoryItem.generated.h"

UINTERFACE(MinimalAPI)
class UInventoryItem : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API IInventoryItem {
    GENERATED_BODY()
  public:
    virtual FDimensions get_size() const = 0;
};
