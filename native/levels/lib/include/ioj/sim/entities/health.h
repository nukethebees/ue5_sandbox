#pragma once

#include <ioj/sim/entities/health_type.h>

namespace ioj::sim {
[[nodiscard]] constexpr auto is_alive(Health const health) noexcept -> bool {
    return health.value() > 0;
}

[[nodiscard]] constexpr auto is_dead(Health const health) noexcept -> bool {
    return health.value() <= 0;
}
} // namespace ioj::sim
