#include "Sandbox/ui/ship_hud/ShipSpeedWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipSpeedWidget::set_speed(float speed) {
    RETURN_IF_NULLPTR(widget);
    widget->update(speed);
}

void UShipSpeedWidget::set_font_size(int32 const new_font_size) {
    RETURN_IF_NULLPTR(widget);
    widget->set_font_size(new_font_size);
}

auto UShipSpeedWidget::get_font_size() const noexcept -> int32 {
    return widget->get_font_size();
}
