#include "Sandbox/widgets/HealthWidget.h"

void UHealthWidget::set_health(FHealthData health_data) {
    if (!health_bar) {
        return;
    }

    health_bar->SetPercent(FMath::Clamp(health_data.percent, 0.0f, 1.0f));

    if (!health_text) {
        return;
    }

    auto const display_value{FMath::RoundToInt(health_data.percent * 100.0f)};
    health_text->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), display_value)));
}
