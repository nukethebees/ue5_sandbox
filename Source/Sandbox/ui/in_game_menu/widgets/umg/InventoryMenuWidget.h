#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "InventoryMenuWidget.generated.h"

class UInventoryComponent;

class UInventoryGridWidget;
class UItemDetailsWidget;
class UTextBlock;
class UHorizontalBox;
class UVerticalBox;
class UGridPanel;

UCLASS()
class SANDBOX_API UInventoryMenuWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void set_inventory(UInventoryComponent& inv);
    void on_widget_selected();
    void update_money_display(int32 money);
  protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UHorizontalBox* main_box{nullptr};

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* sidebar{nullptr};

    UPROPERTY(meta = (BindWidget))
    UGridPanel* numbers_list_panel{nullptr};

    UPROPERTY(meta = (BindWidget))
    UInventoryGridWidget* inventory_grid{nullptr};

    UPROPERTY(meta = (BindWidget))
    UItemDetailsWidget* item_details{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* money_text{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UInventoryComponent* inventory{nullptr};
};
