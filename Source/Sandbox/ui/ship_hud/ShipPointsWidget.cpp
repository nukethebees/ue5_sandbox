#include "Sandbox/ui/ship_hud/ShipPointsWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipPointsWidget::set_points(int32 points) {
    RETURN_IF_NULLPTR(widget);
    widget->update(points);
}
