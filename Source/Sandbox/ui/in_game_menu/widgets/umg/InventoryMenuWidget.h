#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "InventoryMenuWidget.generated.h"

UCLASS()
class SANDBOX_API UInventoryMenuWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;
};
