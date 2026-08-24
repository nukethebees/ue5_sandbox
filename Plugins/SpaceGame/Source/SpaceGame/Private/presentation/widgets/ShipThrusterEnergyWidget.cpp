#include "SpaceGame/presentation/widgets/ShipThrusterEnergyWidget.h"

#include "Components/ProgressBar.h"

#include "SandboxGameShared/utilities/macros/null_checks.hpp"

void UShipThrusterEnergyWidget::set_energy(float energy) {
    RETURN_IF_NULLPTR(energy_bar);
    energy_bar->SetPercent(energy);
}
