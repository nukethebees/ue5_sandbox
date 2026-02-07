#include "Sandbox/ui/ship_hud/ShipHealthWidget.h"

#include "Components/ProgressBar.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipHealthWidget::set_health(float health) {
    RETURN_IF_NULLPTR(health_bar);
    health_bar->SetPercent(health);
}
