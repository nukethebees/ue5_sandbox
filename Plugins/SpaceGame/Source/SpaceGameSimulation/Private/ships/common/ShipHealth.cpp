#include "SpaceGameSimulation/ships/common/ShipHealth.h"

#include <ioj/sim/ship_health.h>

void FShipHealth::clamp_to_max() noexcept {
    health = ::ioj::sim::clamp_health_to_max(health, max_health);
}
