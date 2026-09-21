#pragma once

#include <cstdint>

namespace ioj::sim::collision {
using StaticGeometryIndex = std::int32_t;

inline constexpr StaticGeometryIndex invalid_static_geometry_index{-1};
} // namespace ioj::sim::collision
