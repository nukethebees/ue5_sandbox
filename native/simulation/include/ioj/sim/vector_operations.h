#pragma once

#include <ioj/sim/rotator_math.h>
#include <ioj/sim/rotators3f.h>
#include <ioj/sim/vectors3f.h>
#include <sandbox/core/generated/vector_lerp_kernels.h>
#include <sandbox/core/vector_math.h>

#include <cassert>
#include <cmath>
#include <span>

namespace ioj::sim {
inline void assign_from(Vectors3f& dst, Vectors3fConstView const src) {
    dst.set_num(src.num());
    auto const out{dst.get_view()};
    for (std::int32_t i{}; i < src.num(); ++i) {
        out.set(i, src[i]);
    }
}

inline void fill(Vectors3fView const vectors, float const value) {
    for (std::int32_t i{}; i < vectors.num(); ++i) {
        vectors.set(i, value, value, value);
    }
}

inline void multiply_in_place(Vectors3fView const vectors, float const scale) {
    for (std::int32_t i{}; i < vectors.num(); ++i) {
        vectors.set(i, vectors.xs[i] * scale, vectors.ys[i] * scale, vectors.zs[i] * scale);
    }
}

inline void
    add_scaled_in_place(Vectors3fView const dst, Vectors3fConstView const src, float const scale) {
    assert(dst.num() == src.num());
    for (std::int32_t i{}; i < dst.num(); ++i) {
        dst.set(i,
                dst.xs[i] + src.xs[i] * scale,
                dst.ys[i] + src.ys[i] * scale,
                dst.zs[i] + src.zs[i] * scale);
    }
}

inline void
    direction(Vectors3fView const out, Vectors3fConstView const from, Vectors3fConstView const to) {
    assert(out.num() == from.num());
    assert(from.num() == to.num());
    for (std::int32_t i{}; i < out.num(); ++i) {
        auto const x{to.xs[i] - from.xs[i]};
        auto const y{to.ys[i] - from.ys[i]};
        auto const z{to.zs[i] - from.zs[i]};
        auto const length{std::sqrt(x * x + y * y + z * z)};
        out.set(i,
                length == 0.0f ? 0.0f : x / length,
                length == 0.0f ? 0.0f : y / length,
                length == 0.0f ? 0.0f : z / length);
    }
}

inline void to_rotations(Rotators3f& out, Vectors3fConstView const directions) {
    out.set_num(directions.num());
    auto const rotations{out.get_view()};
    for (std::int32_t i{}; i < directions.num(); ++i) {
        rotations.set(i, direction_to_rotation(directions[i]));
    }
}

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
