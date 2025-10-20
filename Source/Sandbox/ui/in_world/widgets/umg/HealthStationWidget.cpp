#include "Sandbox/ui/in_world/widgets/umg/HealthStationWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "Sandbox/health/data/StationStateData.h"

UFUNCTION()
void UHealthStationWidget::update(FStationStateData const& state_data) {
    if (capacity_text) {
        auto const formatted_text{
            FString::Printf(TEXT("HP: %d"), static_cast<int32>(state_data.remaining_capacity))};
        capacity_text->SetText(FText::FromString(formatted_text));
    }

    if (cooldown_bar) {
        float percent = state_data.cooldown_total > 0.0f
                          ? state_data.cooldown_remaining / state_data.cooldown_total
                          : 0.0f;

        cooldown_bar->SetPercent(percent);
    }
}
