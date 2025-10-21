#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/UniformGridPanel.h"

#include "InventoryGridWidget.generated.h"

UCLASS()
class SANDBOX_API UInventoryGridWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UUniformGridPanel* item_grid{nullptr};
};
