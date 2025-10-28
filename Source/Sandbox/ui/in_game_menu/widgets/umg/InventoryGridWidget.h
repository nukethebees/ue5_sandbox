#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "InventoryGridWidget.generated.h"

class UTexture2D;

class USizeBox;
class UGridPanel;

class UInventorySlotWidget;

class UInventoryComponent;

UCLASS()
class SANDBOX_API UInventoryGridWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"UInventoryGridWidget", LogSandboxUI> {
    GENERATED_BODY()
  protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    virtual bool NativeOnDrop(FGeometry const& InGeometry,
                              FDragDropEvent const& InDragDropEvent,
                              UDragDropOperation* InOperation) override;

    UPROPERTY(meta = (BindWidget))
    UGridPanel* item_grid{nullptr};

    UPROPERTY(meta = (BindWidget))
    USizeBox* size_box{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UInventoryComponent* inventory{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    TSubclassOf<UInventorySlotWidget> slot_class;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
    UTexture2D* fallback_item_image{nullptr};
  public:
    UFUNCTION()
    void on_visibility_changed(ESlateVisibility new_visibility);

    void set_inventory(UInventoryComponent& inv) { inventory = &inv; }
    void refresh_grid();
};
