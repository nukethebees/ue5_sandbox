#include "Sandbox/ui/ship_hud/ShipHealthWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipHealthWidget::set_health(float health) {
    RETURN_IF_NULLPTR(widget);
    widget->update(health);
}
