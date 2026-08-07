#include "Sandbox/ui/ship_hud/ShipPointsWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipPointsWidget::set_points(int32 points) {
    RETURN_IF_NULLPTR(widget);
    widget->update(points);
}

void UShipPointsWidget::set_font_size(int32 const new_font_size) {
    RETURN_IF_NULLPTR(widget);
    widget->set_font_size(new_font_size);
}

auto UShipPointsWidget::get_font_size() const noexcept -> int32 {
    return widget->get_font_size();
}
