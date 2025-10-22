#include "Sandbox/ui/in_game_menu/widgets/umg/InventorySlotWidget.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UInventorySlotWidget::set_image(UTexture2D& img) {
    RETURN_IF_NULLPTR(icon_image);

    icon_image->SetBrushFromTexture(&img);
}
void UInventorySlotWidget::set_aspect_ratio(float ar) {
    RETURN_IF_NULLPTR(root);

    root->SetMinAspectRatio(ar);
    root->SetMaxAspectRatio(ar);
}

void UInventorySlotWidget::NativeConstruct() {
    Super::NativeConstruct();
}
