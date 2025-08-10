#include "Sandbox/widgets/FuelWidget.h"

void UFuelWidget::NativeConstruct() {
    Super::NativeConstruct();
    update_fuel(0.0f);
}

void UFuelWidget::update_fuel(float new_fuel) {
    static auto const fuel_format{FText::FromStringView(TEXT("Fuel: {0}"))};

    if (fuel_text) {
        auto const fuel_display{FText::Format(fuel_format, FText::AsNumber(new_fuel))};
        fuel_text->SetText(fuel_display);
    }
}
