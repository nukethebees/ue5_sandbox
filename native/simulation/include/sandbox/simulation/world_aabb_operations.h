#pragma once

#include "sandbox/simulation/vector_types.h"
#include "sandbox/simulation/world_aabbs.h"

namespace ml::simulation::collision {
inline void
    set(WorldAABBs& aabbs, std::int32_t const index, Vector3f const min, Vector3f const max) {
    aabbs.get_view().columns().set(index, min.X, min.Y, min.Z, max.X, max.Y, max.Z);
}

inline auto add(WorldAABBs& aabbs, Vector3f const min, Vector3f const max) -> std::int32_t {
    auto const index{aabbs.num()};
    aabbs.add_uninitialised(1);
    set(aabbs, index, min, max);
    return index;
}

[[nodiscard]] inline auto min_at(WorldAABBsColumnsConstView const& aabbs, std::int32_t const index)
    -> Vector3f {
    auto const element{static_cast<std::size_t>(index)};
    return make_vector3f(aabbs.min_xs[element], aabbs.min_ys[element], aabbs.min_zs[element]);
}

[[nodiscard]] inline auto max_at(WorldAABBsColumnsConstView const& aabbs, std::int32_t const index)
    -> Vector3f {
    auto const element{static_cast<std::size_t>(index)};
    return make_vector3f(aabbs.max_xs[element], aabbs.max_ys[element], aabbs.max_zs[element]);
}
} // namespace ml::simulation::collision
