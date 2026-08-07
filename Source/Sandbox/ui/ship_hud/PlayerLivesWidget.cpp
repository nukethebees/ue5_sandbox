#include "Sandbox/ui/ship_hud/PlayerLivesWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UPlayerLivesWidget::set_value(int32 value) {
    RETURN_IF_NULLPTR(widget);
    widget->update(value);
}

void UPlayerLivesWidget::set_font_size(int32 const new_font_size) {
    RETURN_IF_NULLPTR(widget);
    widget->set_font_size(new_font_size);
}

auto UPlayerLivesWidget::get_font_size() const noexcept -> int32 {
    return widget->get_font_size();
}
