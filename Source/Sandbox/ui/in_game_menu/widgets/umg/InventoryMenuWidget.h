#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/ui/in_game_menu/widgets/umg/InventoryGridWidget.h"
#include "Sandbox/ui/in_game_menu/widgets/umg/ItemDetailsWidget.h"

#include "InventoryMenuWidget.generated.h"

UCLASS()
class SANDBOX_API UInventoryMenuWidget : public UUserWidget {
    GENERATED_BODY()
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UInventoryGridWidget* inventory_grid{nullptr};

    UPROPERTY(meta = (BindWidget))
    UItemDetailsWidget* item_details{nullptr};
};
