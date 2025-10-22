#include "Sandbox/ui/in_game_menu/widgets/umg/InventorySlotWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UInventorySlotWidget::set_image(UTexture2D& img) {
    RETURN_IF_NULLPTR(icon_image);

    icon_image->SetBrushFromTexture(&img);
}

void UInventorySlotWidget::NativeConstruct() {
    Super::NativeConstruct();
}
