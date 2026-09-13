#include "sandbox/simulation/entity_world_bounds.h"

#include <cmath>

namespace ml::simulation::collision {
namespace {
auto rotate_vector(Quaternion4f const orientation, Vector3f const vector) noexcept -> Vector3f {
    auto const quaternion_vector{make_vector3f(orientation.X, orientation.Y, orientation.Z)};
    auto const twice_cross{HMM_MulV3F(HMM_Cross(quaternion_vector, vector), 2.0f)};
    return HMM_AddV3(HMM_AddV3(vector, HMM_MulV3F(twice_cross, orientation.W)),
                     HMM_Cross(quaternion_vector, twice_cross));
}

auto absolute(Vector3f const vector) noexcept -> Vector3f {
    return make_vector3f(std::abs(vector.X), std::abs(vector.Y), std::abs(vector.Z));
}
}

auto make_entity_world_bounds(EntityAABBs const& bounds,
                              std::int32_t const type_index,
                              Vector3f const position,
                              Quaternion4f const orientation) noexcept -> WorldAABB {
    auto const centre{position + rotate_vector(orientation, bounds.get_centre(type_index))};
    auto const local_extent{bounds.get_half_extents(type_index)};
    auto const extent{
        absolute(rotate_vector(orientation, make_vector3f(local_extent.X, 0.0f, 0.0f))) +
        absolute(rotate_vector(orientation, make_vector3f(0.0f, local_extent.Y, 0.0f))) +
        absolute(rotate_vector(orientation, make_vector3f(0.0f, 0.0f, local_extent.Z)))};
    return {centre - extent, centre + extent};
}
} // namespace ml::simulation::collision
