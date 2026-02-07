#include "Sandbox/ui/ship_hud/ShipThrusterEnergyWidget.h"

#include "Components/ProgressBar.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipThrusterEnergyWidget::set_energy(float energy) {
    RETURN_IF_NULLPTR(energy_bar);
    energy_bar->SetPercent(energy);
}
