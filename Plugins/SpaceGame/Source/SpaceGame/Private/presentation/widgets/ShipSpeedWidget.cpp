#include "SpaceGame/presentation/widgets/ShipSpeedWidget.h"

#include "SandboxGameShared/ui/widgets/ValueWidget.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

void UShipSpeedWidget::set_speed(float speed) {
    RETURN_IF_NULLPTR(widget);
    widget->update(speed);
}

void UShipSpeedWidget::set_font_size(int32 const new_font_size) {
    RETURN_IF_NULLPTR(widget);
    widget->set_font_size(new_font_size);
}

void UShipSpeedWidget::set_text_style(FTextBlockStyle const& style) {
    RETURN_IF_NULLPTR(widget);
    widget->set_text_style(style);
}

auto UShipSpeedWidget::get_font_size() const noexcept -> int32 {
    return widget->get_font_size();
}
