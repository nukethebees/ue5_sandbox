#include "Sandbox/ui/ship_hud/ShipSpeedWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipSpeedWidget::set_speed(float speed) {
    RETURN_IF_NULLPTR(widget);
    widget->update(speed);
}
