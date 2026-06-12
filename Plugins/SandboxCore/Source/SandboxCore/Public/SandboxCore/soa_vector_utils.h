#pragma once

#include <SandboxCore/soa_vectors.h>

#include <HAL/Platform.h>
#include <Math/MathFwd.h>

#include <concepts>

namespace ml::kernel {
template <typename T>
void fill(T* RESTRICT xs, T* RESTRICT ys, T* RESTRICT zs, T const value, int32 const count) {
    for (int32 i{0}; i < count; ++i) {
        xs[i] = value;
        ys[i] = value;
        zs[i] = value;
    }
}
}

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
inline void assign(FVectors3f& vector, int32 const i, FVector const& value) {
    vector.xs[i] = static_cast<float>(value.X);
    vector.ys[i] = static_cast<float>(value.Y);
    vector.zs[i] = static_cast<float>(value.Z);
}

inline void fill(FVectors3f& vector, float const value) {
    ml::kernel::fill(
        vector.xs.GetData(), vector.ys.GetData(), vector.zs.GetData(), value, vector.num());
}
inline void fill(TVectors3View<float> vector, float const value) {
    ml::kernel::fill(
        vector.xs.GetData(), vector.ys.GetData(), vector.zs.GetData(), value, vector.num());
}

// Extension
template <is_vec3f Vec3f>
inline void append(FVectors3f& vector, Vec3f const& to_append) {
    auto const n_base{vector.num()};
    auto const n_to_append{to_append.num()};

    vector.add_uninitialized(n_to_append);
    for (int32 i{0}; i < n_to_append; ++i) {
        auto const index{n_base + i};

        vector.xs[index] = to_append.xs[i];
        vector.ys[index] = to_append.ys[i];
        vector.zs[index] = to_append.zs[i];
    }
}

// Maths
void SANDBOXCORE_API multiply_in_place(FVectors3f& dst, float const value);

// Conversion
inline auto get_fvector(FVectors3f const& src, int32 i) -> FVector {
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
