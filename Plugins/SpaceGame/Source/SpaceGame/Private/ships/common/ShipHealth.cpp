#include "SpaceGame/ships/common/ShipHealth.h"

void FShipHealth::clamp_to_max() noexcept {
    if (max_health > health) {
        health = max_health;
    }
}
