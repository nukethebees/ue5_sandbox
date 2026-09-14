#pragma once

#include "ioj/sim/entity_aabbs.h"
#include "ioj/sim/vector_types.h"

#include <cstdint>

namespace ioj::sim::collision {
struct WorldAABB {
    Vector3f min;
    Vector3f max;
};

[[nodiscard]] auto make_entity_world_bounds(EntityAABBs const& bounds,
                                            std::int32_t type_index,
                                            Vector3f position,
                                            Quaternion4f orientation) noexcept -> WorldAABB;
[[nodiscard]] auto get_entity_radius(EntityAABBs const& bounds, std::int32_t type_index) noexcept
    -> float;
} // namespace ioj::sim::collision
