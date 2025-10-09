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
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TArray<FInventorySlot> slots{};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FDimensions dimensions{0, 0};
};
