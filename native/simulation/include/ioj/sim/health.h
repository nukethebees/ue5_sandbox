#pragma once

#include <cstdint>

namespace ioj::sim {
[[nodiscard]] constexpr auto is_alive(std::int32_t const health) noexcept -> bool {
    return health > 0;
}

[[nodiscard]] constexpr auto is_dead(std::int32_t const health) noexcept -> bool {
    return health <= 0;
}
} // namespace ioj::sim
