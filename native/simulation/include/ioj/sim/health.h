#pragma once

#include <cstdint>

namespace ioj::sim {
using Health = std::int32_t;

[[nodiscard]] constexpr auto is_alive(Health const health) noexcept -> bool {
    return health > 0;
}

[[nodiscard]] constexpr auto is_dead(Health const health) noexcept -> bool {
    return health <= 0;
}
} // namespace ioj::sim
