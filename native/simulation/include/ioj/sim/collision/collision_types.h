#pragma once

#include "ioj/sim/collision/collision_scalar_types.h"

#include <cstdint>
#include <limits>

namespace ioj::sim::collision {
using OverlapEntryCount = std::uint32_t;
using EntityAabbRow = std::uint32_t;

inline constexpr EntityAabbRow invalid_entity_aabb_row{std::numeric_limits<EntityAabbRow>::max()};
inline constexpr StaticGeometryIndex invalid_static_geometry_index{StaticGeometryIndex::Invalid};
inline constexpr CellIndex invalid_cell_index{CellIndex::Invalid};
} // namespace ioj::sim::collision
