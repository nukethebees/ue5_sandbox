#pragma once

#include <SandboxCore/array_utils.h>
#include <SandboxCore/soa_vectors.h>
#include <SandboxCore/vector_math.h>

#include <HAL/Platform.h>
#include <Math/MathFwd.h>

#include <concepts>

namespace ml {
template <typename T>
concept is_vec3f = requires(T const& vecs) {
    { vecs.xs[0] } -> std::convertible_to<float>;
    { vecs.ys[0] } -> std::convertible_to<float>;
    { vecs.zs[0] } -> std::convertible_to<float>;
    { vecs.num() } -> std::convertible_to<int32>;
};

// Assignment
void SANDBOXCORE_API assign_from(FVectors3f& dst, FVectors3f const& src);

template <is_vec3f Vec3f>
inline void assign_from(FVectors3f& dst, int32 const dst_i, Vec3f const& src, int32 const src_i) {
    dst.xs[dst_i] = src.xs[src_i];
    dst.ys[dst_i] = src.ys[src_i];
    dst.zs[dst_i] = src.zs[src_i];
}

inline void assign(FVectors3f& vector, int32 const i, float const value) {
    vector.xs[i] = value;
    vector.ys[i] = value;
    vector.zs[i] = value;
}
inline void assign(FVectors3f& vector, int32 const i, float const x, float const y, float const z) {
    vector.xs[i] = x;
    vector.ys[i] = y;
    vector.zs[i] = z;
}
inline void assign(FVectors3f& vector, int32 const i, FVector const& value) {
    vector.xs[i] = static_cast<float>(value.X);
    vector.ys[i] = static_cast<float>(value.Y);
    vector.zs[i] = static_cast<float>(value.Z);
}
inline void assign(FVectors3f& vector, int32 const i, FVector3f const& value) {
    vector.xs[i] = value.X;
    vector.ys[i] = value.Y;
    vector.zs[i] = value.Z;
}

inline void fill(FVectors3f& vector, float const value) {
    ml::kernel::fill(
        vector.xs.GetData(), vector.ys.GetData(), vector.zs.GetData(), value, vector.num());
}
inline void fill(FVectors3f::View vector, float const value) {
    ml::kernel::fill(
        vector.xs.GetData(), vector.ys.GetData(), vector.zs.GetData(), value, vector.num());
}

// Extension
inline void append(FVectors3f& vector, float const x, float const y, float const z) {
    vector.xs.Add(x);
    vector.ys.Add(y);
    vector.zs.Add(z);
}

template <is_vec3f Vec3f>
inline void append_from(FVectors3f& vector, Vec3f const& to_append) {
    auto const n_base{vector.num()};
    auto const n_to_append{to_append.num()};

    vector.add_uninitialized(n_to_append);
    ml::kernel::assign_from(vector.xs.GetData() + n_base,
                            vector.ys.GetData() + n_base,
                            vector.zs.GetData() + n_base,
                            to_append.xs.GetData(),
                            to_append.ys.GetData(),
                            to_append.zs.GetData(),
                            n_to_append);
}
template <is_vec3f Vec3f>
inline void append_element_from(FVectors3f& vector, Vec3f const& to_append, int32 const i) {
    vector.xs.Add(to_append.xs[i]);
    vector.ys.Add(to_append.ys[i]);
    vector.zs.Add(to_append.zs[i]);
}

// Maths

void SANDBOXCORE_API add_scaled_in_place(FVectors3f& dst,
                                         FVectors3f const& src,
                                         float const scale_factor);
void SANDBOXCORE_API multiply_in_place(FVectors3f& dst, float const value);

inline auto dist_sq(FVectors3f const& vecs, int32 const i, FVector3f const& other) -> float {
    auto const dx{vecs.xs[i] - other.X};
    auto const dy{vecs.ys[i] - other.Y};
    auto const dz{vecs.zs[i] - other.Z};

    return ml::size_squared(dx, dy, dz);
}
inline auto
    dist_sq(FVectors3f const& vecs, int32 const i, float const ox, float const oy, float const oz)
        -> float {
    auto const dx{vecs.xs[i] - ox};
    auto const dy{vecs.ys[i] - oy};
    auto const dz{vecs.zs[i] - oz};

    return ml::size_squared(dx, dy, dz);
}

// Conversion
inline auto get_fvector(FVectors3f const& src, int32 i) -> FVector {
    return {
        static_cast<double>(src.xs[i]),
        static_cast<double>(src.ys[i]),
        static_cast<double>(src.zs[i]),
    };
}
inline auto get_fvector3f(FVectors3f const& src, int32 i) -> FVector3f {
    return {src.xs[i], src.ys[i], src.zs[i]};
}

inline auto scaled_fvector(FVectors3f const& vectors, int32 const i, float const scale) -> FVector {
    return FVector{
        vectors.xs[i] * scale,
        vectors.ys[i] * scale,
        vectors.zs[i] * scale,
    };
}
}

inline auto operator*=(FVectors3f& vectors, float const scale) -> FVectors3f& {
    ml::multiply_in_place(vectors, scale);
    return vectors;
}
