#pragma once

#include "ioj/sim/collision_scalar_types.h"

#include <limits>

namespace ioj::sim::collision {
inline constexpr StaticGeometryIndex invalid_static_geometry_index{
    std::numeric_limits<std::uint32_t>::max()};
inline constexpr CellIndex invalid_cell_index{std::numeric_limits<std::uint32_t>::max()};
} // namespace ioj::sim::collision
