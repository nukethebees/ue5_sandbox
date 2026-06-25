#include "Sandbox/health/ShipHealth.h"

void FShipHealth::clamp_to_max() noexcept {
    if (max_health > health) { health = max_health; }
}
