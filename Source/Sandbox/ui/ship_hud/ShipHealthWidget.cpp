#include "Sandbox/ui/ship_hud/ShipHealthWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Components/ProgressBar.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipHealthWidget::set_health(FShipHealth health) {
    check(health_bar);
    check(health_text);

    health_bar->SetPercent(static_cast<float>(health.health) /
                           static_cast<float>(health.max_health));

    health_text->update(health.health, health.max_health);
}
