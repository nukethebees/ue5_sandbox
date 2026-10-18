#include "ShooterGame/ui/hud/HealthWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "SandboxGameShared/utilities/math.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

void UHealthWidget::set_health(FHealthData health_data) {
    RETURN_IF_NULLPTR(health_bar);
    health_bar->SetPercent(ioj::clamp_normalised_percent(health_data.percent));

    RETURN_IF_NULLPTR(health_text);
    health_text->SetText(FText::FromString(
        FString::Printf(TEXT("%d%%"), ioj::denormalise_percent_to_int(health_data.percent))));
}
