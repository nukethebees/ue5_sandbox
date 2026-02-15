#include "Sandbox/ui/ship_hud/ShipRenderIconWidget.h"

#include "Components/Image.h"
#include "Materials/MaterialInterface.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipRenderIconWidget::NativeConstruct() {
    Super::NativeConstruct();

    RETURN_IF_NULLPTR(item_image);
    RETURN_IF_NULLPTR(item_material);

    item_image->SetBrushFromMaterial(item_material);
}
