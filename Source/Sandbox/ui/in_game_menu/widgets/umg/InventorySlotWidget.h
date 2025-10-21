#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

#include "InventorySlotWidget.generated.h"

UCLASS()
class SANDBOX_API UInventorySlotWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UImage* icon_image{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* stack_size_text{nullptr};
};
