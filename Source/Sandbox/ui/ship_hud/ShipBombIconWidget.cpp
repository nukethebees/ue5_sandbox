#include "Sandbox/ui/ship_hud/ShipBombIconWidget.h"

#include "Components/Image.h"
#include "Materials/MaterialInterface.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipBombIconWidget::NativeConstruct() {
    Super::NativeConstruct();

    RETURN_IF_NULLPTR(bomb_image);
    RETURN_IF_NULLPTR(bomb_material);

    bomb_image->SetBrushFromMaterial(bomb_material);
}
