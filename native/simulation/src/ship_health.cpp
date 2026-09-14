#include "ioj/sim/ship_health.h"

namespace ioj::sim {
auto clamp_health_to_max(std::int32_t const health, std::int32_t const max_health) noexcept
    -> std::int32_t {
    return max_health > health ? max_health : health;
}
} // namespace ioj::sim
