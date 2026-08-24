#include "SpaceGame/presentation/widgets/ShipHealthWidget.h"

#include "SandboxGameShared/ui/widgets/ValueWidget.h"

#include "Components/ProgressBar.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

void UShipHealthWidget::set_health(FShipHealth health) {
    check(health_bar);
    check(health_text);

    health_bar->SetPercent(static_cast<float>(health.health) /
                           static_cast<float>(health.max_health));

    health_text->update(health.health, health.max_health);
}

void UShipHealthWidget::set_font_size(int32 const new_font_size) {
    check(health_text);
    health_text->set_font_size(new_font_size);
}

auto UShipHealthWidget::get_font_size() const noexcept -> int32 {
    return health_text->get_font_size();
}
