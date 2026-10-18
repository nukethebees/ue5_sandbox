#include "ioj/sim/collision/entity_world_bounds.h"

#include <algorithm>
#include <cmath>

namespace ioj::sim::collision {
namespace {
auto absolute(Vector3f const vector) noexcept -> Vector3f {
    return Vector3f{std::abs(vector.x()), std::abs(vector.y()), std::abs(vector.z())};
}
}

auto make_entity_world_bounds(EntityAABBs const& bounds,
                              EntityType const type,
                              Vector3f const position,
                              Quaternion4f const orientation) noexcept -> WorldAABB {
    auto const centre{position + rotate_vector(orientation, bounds.get_centre(type))};
    auto const local_extent{bounds.get_half_extents(type)};
    auto const extent{absolute(rotate_vector(orientation, Vector3f{local_extent.x(), 0.0f, 0.0f})) +
                      absolute(rotate_vector(orientation, Vector3f{0.0f, local_extent.y(), 0.0f})) +
                      absolute(rotate_vector(orientation, Vector3f{0.0f, 0.0f, local_extent.z()}))};
    return {centre - extent, centre + extent};
}

auto get_entity_radius(EntityAABBs const& bounds, EntityType const type) noexcept -> float {
    auto const half_extents{bounds.get_half_extents(type)};
    return std::max({half_extents.x(), half_extents.y(), half_extents.z()});
}
} // namespace collision
