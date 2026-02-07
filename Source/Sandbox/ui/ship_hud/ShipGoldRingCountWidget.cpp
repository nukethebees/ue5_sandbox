#include "Sandbox/ui/ship_hud/ShipGoldRingCountWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipGoldRingCountWidget::set_count(int32 count) {
    RETURN_IF_NULLPTR(widget);
    widget->update(count);
}
