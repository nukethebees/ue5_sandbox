#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"

#include "Sandbox/inventory/data/Coord.h"

#include "InventorySlotDragDropOperation.generated.h"

struct FInventorySlot;

UCLASS()
class UInventorySlotDragDropOperation : public UDragDropOperation {
    GENERATED_BODY()
  public:
    UInventorySlotDragDropOperation() = default;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    FCoord click_location;
  protected:
    FInventorySlot const* inventory_slot{nullptr};
};
