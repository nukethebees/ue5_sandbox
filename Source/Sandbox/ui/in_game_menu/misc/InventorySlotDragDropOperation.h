#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"

#include "Sandbox/inventory/Coord.h"

#include "InventorySlotDragDropOperation.generated.h"

struct FInventoryEntry;

UCLASS()
class UInventorySlotDragDropOperation : public UDragDropOperation {
    GENERATED_BODY()
  public:
    UInventorySlotDragDropOperation() = default;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    FCoord click_global_location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    FCoord click_local_location;

    FInventoryEntry const* inventory_slot{nullptr};
};
