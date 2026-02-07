#include "Sandbox/ui/ship_hud/ShipThrusterEnergyWidget.h"

#include "Sandbox/ui/widgets/ValueWidget.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UShipThrusterEnergyWidget::set_energy(float energy) {
    RETURN_IF_NULLPTR(widget);
    widget->update(energy);
}
