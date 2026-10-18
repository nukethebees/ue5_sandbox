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

[[nodiscard]] inline auto min_at(WorldAABBs::ConstView const aabbs, std::uint32_t const index)
    -> Vector3f {
    auto const element{static_cast<std::size_t>(index)};
    return ioj::Vector3f{aabbs.min_xs()[element], aabbs.min_ys()[element], aabbs.min_zs()[element]};
}

[[nodiscard]] inline auto max_at(WorldAABBs::ConstView const aabbs, std::uint32_t const index)
    -> Vector3f {
    auto const element{static_cast<std::size_t>(index)};
    return ioj::Vector3f{aabbs.max_xs()[element], aabbs.max_ys()[element], aabbs.max_zs()[element]};
}
} // namespace ioj::sim::collision
