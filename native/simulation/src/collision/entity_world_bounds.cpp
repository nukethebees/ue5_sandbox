#include "ioj/sim/collision/entity_world_bounds.h"

#include <ioj/vector_value_math.h>

#include <algorithm>

namespace ioj::sim::collision {
auto make_entity_world_bounds(EntityAABBs const& bounds,
                              EntityType const type,
                              Vector3f const position,
                              Quaternion4f const orientation) noexcept -> WorldAABB {
    auto const centre{position + rotate_vector(orientation, bounds.centre(type))};
    auto const local_extent{bounds.half_extents(type)};
    auto const extent{abs(rotate_vector(orientation, Vector3f{local_extent.x(), 0.0f, 0.0f})) +
                      abs(rotate_vector(orientation, Vector3f{0.0f, local_extent.y(), 0.0f})) +
                      abs(rotate_vector(orientation, Vector3f{0.0f, 0.0f, local_extent.z()}))};
    return {centre - extent, centre + extent};
}

auto get_entity_radius(EntityAABBs const& bounds, EntityType const type) noexcept -> float {
    auto const half_extents{bounds.half_extents(type)};
    return std::max({half_extents.x(), half_extents.y(), half_extents.z()});
}
} // namespace collision
