#include "Sandbox/ui/ship_hud/ShipBombCountWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipBombCountWidget::set_count(int32 count) {
    RETURN_IF_NULLPTR(widget);
    widget->update(count);
}
