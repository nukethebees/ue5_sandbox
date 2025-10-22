#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"

#include "InventorySlotDragDropOperation.generated.h"

UCLASS()
class UInventorySlotDragDropOperation : public UDragDropOperation {
    GENERATED_BODY()
  public:
    UInventorySlotDragDropOperation() = default;
};
