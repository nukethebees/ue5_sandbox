#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace ioj::levels {
// Return zero for non-positive/non-finite inputs or an unrepresentable dimension.
[[nodiscard]] inline auto grid_axis_count(float const extent, float const cell) noexcept
    -> std::int32_t {
    if (!std::isfinite(extent) || !std::isfinite(cell) || extent <= 0.0f || cell <= 0.0f) {
        return 0;
    }
    auto const count{std::ceil(static_cast<double>(extent) / cell)};
    if (count > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
        return 0;
    }
    return static_cast<std::int32_t>(count);
}
[[nodiscard]] constexpr auto grid_cell_count_fits(std::int32_t const x,
                                                  std::int32_t const y,
                                                  std::int32_t const z) noexcept -> bool {
    return x > 0 && y > 0 && z > 0 &&
           static_cast<std::int64_t>(x) * y <= std::numeric_limits<std::int32_t>::max() / z;
}
}
