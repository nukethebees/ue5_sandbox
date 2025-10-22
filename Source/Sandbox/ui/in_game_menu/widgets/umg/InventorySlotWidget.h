#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "InventorySlotWidget.generated.h"

class UTexture2D;

class UImage;
class UTextBlock;

UCLASS()
class SANDBOX_API UInventorySlotWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_image(UTexture2D& img);
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UImage* icon_image{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* stack_size_text{nullptr};
};
