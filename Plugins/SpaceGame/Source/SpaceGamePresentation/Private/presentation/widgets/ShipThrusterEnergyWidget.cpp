#include "SpaceGamePresentation/presentation/widgets/ShipThrusterEnergyWidget.h"

#include "Components/ProgressBar.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

void UShipThrusterEnergyWidget::set_energy(float energy) {
    RETURN_IF_NULLPTR(energy_bar);
    energy_bar->SetPercent(energy);
}

void UShipThrusterEnergyWidget::apply_hud_style(ml::ioj::FGameHudStyle const& style) {
    RETURN_IF_NULLPTR(energy_bar);
    energy_bar->SetWidgetStyle(style.energy_bar);
    energy_bar->SetFillColorAndOpacity(style.energy);
}
