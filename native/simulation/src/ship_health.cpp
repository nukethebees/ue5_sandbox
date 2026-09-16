#include "ioj/sim/ship_health.h"

namespace ioj::sim {
auto clamp_health_to_max(Health const health, Health const max_health) noexcept -> Health {
    return max_health > health ? max_health : health;
}
} // namespace ioj::sim
