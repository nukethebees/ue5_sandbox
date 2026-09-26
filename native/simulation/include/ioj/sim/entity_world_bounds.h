#pragma once

#include "ioj/sim/entity_aabbs.h"
#include "ioj/sim/entity_type.h"
#include "ioj/sim/vector_types.h"
#include <ioj/sim/collision/world_aabb.h>

namespace ioj::sim::collision {

[[nodiscard]] auto make_entity_world_bounds(EntityAABBs const& bounds,
                                            EntityType type,
                                            Vector3f position,
                                            Quaternion4f orientation) noexcept -> WorldAABB;
[[nodiscard]] auto get_entity_radius(EntityAABBs const& bounds, EntityType type) noexcept -> float;
} // namespace ioj::sim::collision
