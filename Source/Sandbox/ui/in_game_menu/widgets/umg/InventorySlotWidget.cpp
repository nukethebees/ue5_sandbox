#include "Sandbox/ui/in_game_menu/widgets/umg/InventorySlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

#include "Sandbox/ui/in_game_menu/misc/InventorySlotDragDropOperation.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UInventorySlotWidget::set_image(UTexture2D& img) {
    RETURN_IF_NULLPTR(icon_image);

    set_image_visibility(true);
    icon_image->SetBrushFromTexture(&img);
}
void UInventorySlotWidget::set_stack_text(FText const& text) {
    check(stack_size_text);
    set_text_visibility(true);
    stack_size_text->SetText(text);
    align_stack_text(*stack_size_text);
}
void UInventorySlotWidget::set_no_image_fallback_text(FText const& text) {
    check(stack_size_text);
    set_text_visibility(true);
    stack_size_text->SetText(text);
    align_fallback_text(*stack_size_text);
}

void UInventorySlotWidget::set_aspect_ratio(float ar) {
    RETURN_IF_NULLPTR(root);

    root->SetMinAspectRatio(ar);
    root->SetMaxAspectRatio(ar);
}
void UInventorySlotWidget::set_text_visibility(bool vis) {
    check(stack_size_text);
    stack_size_text->SetVisibility(vis ? ESlateVisibility::SelfHitTestInvisible
                                       : ESlateVisibility::Collapsed);
}
void UInventorySlotWidget::set_image_visibility(bool vis) {
    check(icon_image);
    icon_image->SetVisibility(vis ? ESlateVisibility::SelfHitTestInvisible
                                  : ESlateVisibility::Collapsed);
}

void UInventorySlotWidget::NativeConstruct() {
    Super::NativeConstruct();
}
void UInventorySlotWidget::NativeOnDragDetected(FGeometry const& InGeometry,
                                                FPointerEvent const& InMouseEvent,
                                                UDragDropOperation*& OutOperation) {
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
    log_verbose(TEXT("NativeOnDragDetected"));

    TRY_INIT_PTR(world, GetWorld());

    auto* drag_operation{NewObject<UInventorySlotDragDropOperation>(this)};
    OutOperation = drag_operation;
}
void UInventorySlotWidget::NativeOnDragEnter(FGeometry const& InGeometry,
                                             FDragDropEvent const& InDragDropEvent,
                                             UDragDropOperation* InOperation) {
    Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
    log_verbose(TEXT("NativeOnDragEnter"));
}
void UInventorySlotWidget::NativeOnDragLeave(FDragDropEvent const& InDragDropEvent,
                                             UDragDropOperation* InOperation) {
    Super::NativeOnDragLeave(InDragDropEvent, InOperation);
    log_verbose(TEXT("NativeOnDragLeave"));
}
bool UInventorySlotWidget::NativeOnDragOver(FGeometry const& InGeometry,
                                            FDragDropEvent const& InDragDropEvent,
                                            UDragDropOperation* InOperation) {
    Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
    log_verbose(TEXT("NativeOnDragOver"));
    return false;
}
FReply UInventorySlotWidget::NativeOnMouseMove(FGeometry const& InGeometry,
                                               FPointerEvent const& InMouseEvent) {
    Super::NativeOnMouseMove(InGeometry, InMouseEvent);
    return FReply::Unhandled();
}
FReply UInventorySlotWidget::NativeOnMouseButtonDown(FGeometry const& InGeometry,
                                                     FPointerEvent const& InMouseEvent) {
    Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    log_verbose(TEXT("NativeOnMouseButtonDown"));

    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton)) {
        return UWidgetBlueprintLibrary::DetectDragIfPressed(
                   InMouseEvent, this, EKeys::LeftMouseButton)
            .NativeReply;
    }

    return FReply::Unhandled();
}
FReply UInventorySlotWidget::NativeOnMouseButtonUp(FGeometry const& InGeometry,
                                                   FPointerEvent const& InMouseEvent) {
    Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
    log_verbose(TEXT("NativeOnMouseButtonUp"));
    return FReply::Unhandled();
}

void UInventorySlotWidget::align_stack_text(UTextBlock& tb) {
    if (auto* overlay_slot{Cast<UOverlaySlot>(tb.Slot)}) {
        overlay_slot->SetHorizontalAlignment(HAlign_Right);
        overlay_slot->SetVerticalAlignment(VAlign_Bottom);
        overlay_slot->SetPadding(FMargin(0));
    }
}
void UInventorySlotWidget::align_fallback_text(UTextBlock& tb) {
    if (auto* overlay_slot{Cast<UOverlaySlot>(tb.Slot)}) {
        overlay_slot->SetHorizontalAlignment(HAlign_Center);
        overlay_slot->SetVerticalAlignment(VAlign_Center);
        overlay_slot->SetPadding(FMargin(0));
    }
}
