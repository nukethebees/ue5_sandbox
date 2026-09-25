#pragma once
#include <ioj/sim/column_math.h>

#include <ioj/sim/rotator_math.h>
#include <ioj/sim/rotators3f.h>
#include <ioj/sim/vectors3f.h>
#include <sandbox/core/generated/vector_lerp_kernels.h>
#include <sandbox/core/vector_math.h>

#include <cassert>
#include <cmath>
#include <span>

namespace ioj::sim {
inline void copy_vectors(VectorColumns auto const& destination, VectorColumns auto const& source) {
    assert(destination.num() == source.num());
    std::ranges::copy(source.xs(), destination.xs().begin());
    std::ranges::copy(source.ys(), destination.ys().begin());
    std::ranges::copy(source.zs(), destination.zs().begin());
}
inline void assign_from(Vectors3f& dst, VectorColumns auto const& src) {
    dst.set_num(src.num());
    copy_vectors(dst.get_view(), src);
}

inline void fill(VectorColumns auto const& vectors, float const value) {
    std::ranges::fill(vectors.xs(), value);
    std::ranges::fill(vectors.ys(), value);
    std::ranges::fill(vectors.zs(), value);
}

inline void multiply_in_place(VectorColumns auto const& vectors, float const scale) {
    auto const xs{vectors.xs()};
    auto const ys{vectors.ys()};
    auto const zs{vectors.zs()};
    auto const count{vectors.num()};
    for (std::int32_t i{}; i < count; ++i) {
        xs[i] *= scale;
        ys[i] *= scale;
        zs[i] *= scale;
    }
}

inline void add_scaled_in_place(VectorColumns auto const& dst,
                                VectorColumns auto const& src,
                                float const scale) {
    assert(dst.num() == src.num());
    auto const dst_xs{dst.xs()};
    auto const dst_ys{dst.ys()};
    auto const dst_zs{dst.zs()};
    auto const src_xs{src.xs()};
    auto const src_ys{src.ys()};
    auto const src_zs{src.zs()};
    auto const count{dst.num()};
    for (std::int32_t i{}; i < count; ++i) {
        dst_xs[i] += src_xs[i] * scale;
        dst_ys[i] += src_ys[i] * scale;
        dst_zs[i] += src_zs[i] * scale;
    }
}

inline void direction(VectorColumns auto const& out,
                      VectorColumns auto const& from,
                      VectorColumns auto const& to) {
    assert(out.num() == from.num());
    assert(from.num() == to.num());
    auto const out_xs{out.xs()};
    auto const out_ys{out.ys()};
    auto const out_zs{out.zs()};
    auto const from_xs{from.xs()};
    auto const from_ys{from.ys()};
    auto const from_zs{from.zs()};
    auto const to_xs{to.xs()};
    auto const to_ys{to.ys()};
    auto const to_zs{to.zs()};
    auto const count{out.num()};
    for (std::int32_t i{}; i < count; ++i) {
        auto const x{to_xs[i] - from_xs[i]};
        auto const y{to_ys[i] - from_ys[i]};
        auto const z{to_zs[i] - from_zs[i]};
        auto const length{std::sqrt(x * x + y * y + z * z)};
        out_xs[i] = length == 0.0f ? 0.0f : x / length;
        out_ys[i] = length == 0.0f ? 0.0f : y / length;
        out_zs[i] = length == 0.0f ? 0.0f : z / length;
    }
}

inline void to_rotations(Rotators3f& out, VectorColumns auto const& directions) {
    out.set_num(directions.num());
    auto const rotations{out.get_view()};
    for (std::int32_t i{}; i < directions.num(); ++i) {
        rotations.set(i, direction_to_rotation(vector_at(directions, i)));
    }
}

inline void lerp_in_place(VectorColumns auto const& current,
                          VectorColumns auto const& target,
                          float const alpha) {
    ml::lerp_3d_in_place(
        current.xs(), current.ys(), current.zs(), target.xs(), target.ys(), target.zs(), alpha);
}

inline void distance_and_squared(std::span<float> const distances,
                                 std::span<float> const squared_distances,
                                 VectorColumns auto const& from,
                                 VectorColumns auto const& to) {
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
                                                 from.xs().data(),
                                                 from.ys().data(),
                                                 from.zs().data(),
                                                 to.xs().data(),
                                                 to.ys().data(),
                                                 to.zs().data(),
                                                 count);
}
}
