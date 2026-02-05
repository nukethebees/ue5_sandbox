#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "InventorySlotWidget.generated.h"

struct FInventoryEntry;

class UTexture2D;

class UImage;
class UTextBlock;
class USizeBox;

UCLASS()
class SANDBOX_API UInventorySlotWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"UInventorySlotWidget", LogSandboxUI> {
    GENERATED_BODY()
  public:
    void set_inventory_slot(FInventoryEntry const& new_inventory_slot) {
        inventory_slot = &new_inventory_slot;
    }
    void set_stack_size(int32 size);
    void set_image(UTexture2D& img);

    void set_no_image_fallback_text(FText const& text);

    void set_aspect_ratio(float ar);
    void set_stack_text_visibility(bool vis);
    void set_fallback_text_visibility(bool vis);
    void set_image_visibility(bool vis);
  protected:
    virtual void NativeConstruct() override;

    virtual void NativeOnDragDetected(FGeometry const& InGeometry,
                                      FPointerEvent const& InMouseEvent,
                                      UDragDropOperation*& OutOperation) override;
    virtual FReply NativeOnMouseButtonDown(FGeometry const& InGeometry,
                                           FPointerEvent const& InMouseEvent) override;

    void set_stack_text(FText const& text);
    static void align_stack_text(UTextBlock& tb);
    static void align_fallback_text(UTextBlock& tb);

    UPROPERTY(meta = (BindWidget))
    USizeBox* root{nullptr};

    UPROPERTY(meta = (BindWidget))
    UImage* icon_image{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* stack_size_text{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* fallback_text{nullptr};

    FInventoryEntry const* inventory_slot{nullptr};
};
