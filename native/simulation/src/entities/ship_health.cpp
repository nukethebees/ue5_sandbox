#include "ioj/sim/entities/ship_health.h"

namespace ioj::sim {
void ShipHealth::clamp_to_max() noexcept {
    health = max_health > health ? max_health : health;
}
} // namespace ioj::sim
