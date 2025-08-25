#include "Sandbox/widgets/HealthWidget.h"

void UHealthWidget::set_health_percent(float percent) {
    if (!health_bar) {
        return;
    }

    health_bar->SetPercent(FMath::Clamp(percent, 0.0f, 1.0f));
}
