#pragma once

#include <ioj/sim/vectors3f.h>
#include <sandbox/core/generated/vector_lerp_kernels.h>
#include <sandbox/core/vector_math.h>

#include <cassert>
#include <span>

namespace ioj::sim {
inline void
    lerp_in_place(Vectors3fView const current, Vectors3fConstView const target, float const alpha) {
    ml::lerp_3d_in_place(current.xs_span(),
                         current.ys_span(),
                         current.zs_span(),
                         target.xs_span(),
                         target.ys_span(),
                         target.zs_span(),
                         alpha);
}

inline void distance_and_squared(std::span<float> const distances,
                                 std::span<float> const squared_distances,
                                 Vectors3fConstView const from,
                                 Vectors3fConstView const to) {
    auto const count{from.num()};
    assert(to.num() == count);
    assert(distances.size() >= static_cast<std::size_t>(count));
    assert(squared_distances.size() >= static_cast<std::size_t>(count));
    if (count == 0) {
        return;
    }
    assert(distances.data() != squared_distances.data());
    ml::native_math::distance_and_squared_vector(distances.data(),
                                                 squared_distances.data(),
                                                 from.xs,
                                                 from.ys,
                                                 from.zs,
                                                 to.xs,
                                                 to.ys,
                                                 to.zs,
                                                 count);
}
}
