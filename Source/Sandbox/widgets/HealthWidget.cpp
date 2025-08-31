#include "Sandbox/widgets/HealthWidget.h"

#include "Sandbox/utilities/math.h"

void UHealthWidget::set_health(FHealthData health_data) {
    if (!health_bar) {
        return;
    }

    health_bar->SetPercent(ml::clamp_normalised_percent(health_data.percent));

    if (!health_text) {
        return;
    }

    health_text->SetText(FText::FromString(
        FString::Printf(TEXT("%d%%"), ml::denormalise_percent_to_int(health_data.percent))));
}
