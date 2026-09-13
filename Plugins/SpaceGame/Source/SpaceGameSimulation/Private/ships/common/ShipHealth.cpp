#include "SpaceGameSimulation/ships/common/ShipHealth.h"

#include <sandbox/simulation/ship_health.h>

void FShipHealth::clamp_to_max() noexcept {
    health = ml::simulation::clamp_health_to_max(health, max_health);
}
