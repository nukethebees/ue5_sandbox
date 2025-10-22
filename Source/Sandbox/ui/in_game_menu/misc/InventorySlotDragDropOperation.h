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
    FCoord click_global_location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    FCoord click_local_location;

    FInventorySlot const* inventory_slot{nullptr};
};
