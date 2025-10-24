#include "Sandbox/ui/in_game_menu/widgets/umg/InventorySlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

#include "Sandbox/inventory/data/InventoryEntry.h"
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

    if (!inventory_slot) {
        return;
    }

    RETURN_IF_NULLPTR(stack_size_text);
    RETURN_IF_NULLPTR(icon_image);
    RETURN_IF_NULLPTR(inventory_slot->item);

    set_aspect_ratio(inventory_slot->aspect_ratio());

    auto const& item{*inventory_slot->item};
    if (auto* img{item.get_display_image()}) {
        set_image(*img);
    } else {
        log_warning(TEXT("Item %s has no display image."), *item.get_name());
        set_no_image_fallback_text(FText::FromString(item.get_name()));
    }
}
void UInventorySlotWidget::NativeOnDragDetected(FGeometry const& InGeometry,
                                                FPointerEvent const& InMouseEvent,
                                                UDragDropOperation*& OutOperation) {
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
    log_verbose(TEXT("NativeOnDragDetected"));

    if (!inventory_slot) {
        return;
    }

    TRY_INIT_PTR(world, GetWorld());

    auto const mouse_pos{InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition())};
    auto const widget_size{InGeometry.GetLocalSize()};

    auto const dimensions{inventory_slot->dimensions()};
    auto const w{static_cast<float>(dimensions.x())};
    auto const h{static_cast<float>(dimensions.y())};

    auto const cell_width{widget_size.X / w};
    auto const cell_height{widget_size.Y / h};

    auto const click_col_local{static_cast<int32>(mouse_pos.X / cell_width)};
    auto const click_row_local{static_cast<int32>(mouse_pos.Y / cell_height)};

    auto const item_origin{inventory_slot->origin};
    auto const click_col_grid{item_origin.x() + click_col_local};
    auto const click_row_grid{item_origin.y() + click_row_local};

#if !UE_BUILD_SHIPPING
    auto const widget_pos{InGeometry.AbsoluteToLocal(InGeometry.GetAbsolutePosition())};
    log_verbose(TEXT("\nMouse pos: %s"
                     "\nWidget pos: %s"
                     "\nWidget size: %s"
                     "\ndimensions: %s"
                     "\nLocal pos: (%d, %d)"
                     "\nGrid pos: (%d, %d)"),
                *mouse_pos.ToString(),
                *widget_pos.ToString(),
                *widget_size.ToString(),
                *dimensions.to_string(),
                click_col_local,
                click_row_local,
                click_col_grid,
                click_row_grid);
#endif

    auto* drag_operation{NewObject<UInventorySlotDragDropOperation>(this)};
    check(drag_operation);
    drag_operation->click_global_location = FCoord{click_col_grid, click_row_grid};
    drag_operation->click_local_location = FCoord{click_col_local, click_row_local};
    drag_operation->inventory_slot = inventory_slot;

    OutOperation = drag_operation;
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
