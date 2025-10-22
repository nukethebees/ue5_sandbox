#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "InventoryMenuWidget.generated.h"

class UInventoryComponent;

class UInventoryGridWidget;
class UItemDetailsWidget;

UCLASS()
class SANDBOX_API UInventoryMenuWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_inventory(UInventoryComponent& inv);
    void on_widget_selected();
  protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UInventoryGridWidget* inventory_grid{nullptr};

    UPROPERTY(meta = (BindWidget))
    UItemDetailsWidget* item_details{nullptr};
};
