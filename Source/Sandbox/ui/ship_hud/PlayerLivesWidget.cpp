#include "Sandbox/ui/ship_hud/PlayerLivesWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UPlayerLivesWidget::set_value(int32 value) {
    RETURN_IF_NULLPTR(widget);
    widget->update(value);
}
