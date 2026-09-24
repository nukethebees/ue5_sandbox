#pragma once

#include <ioj/sim/health_type.h>

namespace ioj::sim {
[[nodiscard]] constexpr auto is_alive(Health const health) noexcept -> bool {
    return health > 0;
}

[[nodiscard]] constexpr auto is_dead(Health const health) noexcept -> bool {
    return health <= 0;
}
} // namespace ioj::sim
