#include "SGLegacy/ui/ShipRenderIconWidget.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

#include "Components/Image.h"
#include "Materials/MaterialInterface.h"

void UShipRenderIconWidget::NativeConstruct() {
    Super::NativeConstruct();

    RETURN_IF_NULLPTR(item_image);
    RETURN_IF_NULLPTR(item_material);

    item_image->SetBrushFromMaterial(item_material);
}
