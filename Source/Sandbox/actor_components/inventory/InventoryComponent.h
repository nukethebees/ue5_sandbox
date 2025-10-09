#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Sandbox/data/inventory/InventorySlot.h"
#include "Sandbox/interfaces/inventory/InventoryItem.h"
#include "Sandbox/typedefs/Inventory.h"

#include "InventoryComponent.generated.h"

UCLASS()
class SANDBOX_API UInventoryComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UInventoryComponent();
  protected:
    UPROPERTY()
    TArray<FInventorySlot> slots{};
    UPROPERTY()
    FDimensions dimensions{};
};
