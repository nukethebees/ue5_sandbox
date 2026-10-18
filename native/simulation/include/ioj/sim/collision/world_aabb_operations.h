#pragma once

#include "ioj/math_types.h"
#include "ioj/sim/collision/world_aabbs.h"

namespace ioj::sim::collision {
inline void
    set(WorldAABBs& aabbs, std::uint32_t const index, Vector3f const min, Vector3f const max) {
    auto const view{aabbs.get_view()};
    view.min_xs()[index] = min.x();
    view.min_ys()[index] = min.y();
    view.min_zs()[index] = min.z();
    view.max_xs()[index] = max.x();
    view.max_ys()[index] = max.y();
    view.max_zs()[index] = max.z();
}

inline auto add(WorldAABBs& aabbs, Vector3f const min, Vector3f const max) -> std::uint32_t {
    auto const index{aabbs.num()};
    aabbs.add_uninitialised(1);
    set(aabbs, index, min, max);
    return index;
}

} // namespace ioj::sim::collision
