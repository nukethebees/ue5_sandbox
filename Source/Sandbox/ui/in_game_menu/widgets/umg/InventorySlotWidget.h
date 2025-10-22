#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "InventorySlotWidget.generated.h"

class UTexture2D;

class UImage;
class UTextBlock;
class USizeBox;

UCLASS()
class SANDBOX_API UInventorySlotWidget
    : public UUserWidget
    , public ml::LogMsgMixin<"UInventoryGridWidget", LogSandboxUI> {
    GENERATED_BODY()
  public:
    void set_image(UTexture2D& img);
    void set_stack_text(FText const& text);
    void set_no_image_fallback_text(FText const& text);

    void set_aspect_ratio(float ar);
    void set_text_visibility(bool vis);
    void set_image_visibility(bool vis);
  protected:
    virtual void NativeConstruct() override;
    virtual void NativeOnDragDetected(FGeometry const& InGeometry,
                                      FPointerEvent const& InMouseEvent,
                                      UDragDropOperation*& OutOperation) override;
    virtual void NativeOnDragEnter(FGeometry const& InGeometry,
                                   FDragDropEvent const& InDragDropEvent,
                                   UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(FDragDropEvent const& InDragDropEvent,
                                   UDragDropOperation* InOperation) override;
    virtual bool NativeOnDragOver(FGeometry const& InGeometry,
                                  FDragDropEvent const& InDragDropEvent,
                                  UDragDropOperation* InOperation) override;
    virtual FReply NativeOnMouseMove(FGeometry const& InGeometry,
                                     FPointerEvent const& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(FGeometry const& InGeometry,
                                           FPointerEvent const& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(FGeometry const& InGeometry,
                                         FPointerEvent const& InMouseEvent) override;

    static void align_stack_text(UTextBlock& tb);
    static void align_fallback_text(UTextBlock& tb);

    UPROPERTY(meta = (BindWidget))
    USizeBox* root{nullptr};

    UPROPERTY(meta = (BindWidget))
    UImage* icon_image{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* stack_size_text{nullptr};
};
