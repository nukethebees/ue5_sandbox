#pragma once

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>
#include <SandboxCore/container_traits.h>
#include <SandboxCore/detail/interpolation.tpp>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vector_concepts.h>
#include <SandboxCore/soa_vectors.h>
#include <SandboxCore/vector_math.h>

#include <HAL/Platform.h>
#include <Math/MathFwd.h>
#include <Math/UnrealMathUtility.h>

#include <concepts>
#include <initializer_list>

namespace ml {
void SANDBOXCORE_API check_is_consistent(FVectors3f const& vec);

/* ---------------------------------------------------------------------------------------------- */
// Comparison
/* ---------------------------------------------------------------------------------------------- */
[[nodiscard]]
inline auto SANDBOXCORE_API almost_equal(FVectors3f const& a,
                                         FVectors3f const& b,
                                         float const tolerance = KINDA_SMALL_NUMBER) -> bool {
    auto const n{ml::num(a)};
    if (n != ml::num(b)) {
        return false;
    }

    if (n == 0) {
        return true;
    }

    return ml::kernel::almost_equal(a.xs.GetData(), b.xs.GetData(), n, tolerance) &&
           ml::kernel::almost_equal(a.ys.GetData(), b.ys.GetData(), n, tolerance) &&
           ml::kernel::almost_equal(a.zs.GetData(), b.zs.GetData(), n, tolerance);
}

/* ---------------------------------------------------------------------------------------------- */
// Construction
/* ---------------------------------------------------------------------------------------------- */
[[nodiscard]]
inline auto SANDBOXCORE_API make_vectors3f(std::initializer_list<float> const xs,
                                           std::initializer_list<float> const ys,
                                           std::initializer_list<float> const zs) -> FVectors3f {
    auto const n{xs.size()};
    check(n == ys.size());
    check(n == zs.size());

    return {
        .xs = xs,
        .ys = ys,
        .zs = zs,
    };
}
[[nodiscard]]
inline auto SANDBOXCORE_API make_vectors3f(TArray<float> xs, TArray<float> ys, TArray<float> zs)
    -> FVectors3f {
    check(ml::all_num_equal(xs, ys, zs));

    return {
        .xs = MoveTemp(xs),
        .ys = MoveTemp(ys),
        .zs = MoveTemp(zs),
    };
}
[[nodiscard]]
inline auto SANDBOXCORE_API make_vectors3f(TArray<FVector3f> const& vectors) -> FVectors3f {
    FVectors3f out;
    out.reserve(vectors.Num());

    for (auto const& vector : vectors) {
        out.add(vector);
    }

    return out;
}

/* ---------------------------------------------------------------------------------------------- */
// Assignment
/* ---------------------------------------------------------------------------------------------- */
void SANDBOXCORE_API assign_from(FVectors3f& dst, FVectors3f const& src);
void SANDBOXCORE_API assign_from(FVectors3f& dst, FVectors3f::ConstView src);

template <is_mutable_vec3f DstT, is_readable_vec3f SrcT>
inline void assign_from(DstT&& dst, int32 const dst_i, SrcT&& src, int32 const src_i) {
    dst.xs.GetData()[dst_i] = src.xs.GetData()[src_i];
    dst.ys.GetData()[dst_i] = src.ys.GetData()[src_i];
    dst.zs.GetData()[dst_i] = src.zs.GetData()[src_i];
}

template <is_mutable_vec3f DstT>
void assign(DstT&& vector, int32 const i, float const value) {
    vector.xs[i] = value;
    vector.ys[i] = value;
    vector.zs[i] = value;
}

template <is_mutable_vec3f DstT>
void assign(DstT&& vector, int32 const i, float const x, float const y, float const z) {
    vector.xs[i] = x;
    vector.ys[i] = y;
    vector.zs[i] = z;
}

template <is_mutable_vec3f DstT>
void assign(DstT&& vector, int32 const i, FVector const& value) {
    vector.xs[i] = static_cast<float>(value.X);
    vector.ys[i] = static_cast<float>(value.Y);
    vector.zs[i] = static_cast<float>(value.Z);
}

template <is_mutable_vec3f DstT>
void assign(DstT&& vector, int32 const i, FVector3f const& value) {
    vector.xs[i] = value.X;
    vector.ys[i] = value.Y;
    vector.zs[i] = value.Z;
}

template <is_mutable_vec3f DstT, is_readable_vec3f SrcT>
void assign_from(DstT&& dst, SrcT&& src) {
    auto const n{dst.num()};
    check(ml::all_num_equal_and_pointers_not_equal(dst, src));
    if (n < 1) {
        return;
    }

    ml::kernel::assign_from(dst.xs.GetData(),
                            dst.ys.GetData(),
                            dst.zs.GetData(),
                            src.xs.GetData(),
                            src.ys.GetData(),
                            src.zs.GetData(),
                            n);
}

inline void fill(FVectors3f& vector, float const value) {
    ml::kernel::fill(
        vector.xs.GetData(), vector.ys.GetData(), vector.zs.GetData(), value, vector.num());
}
inline void fill(FVectors3f::View vector, float const value) {
    ml::kernel::fill(
        vector.xs.GetData(), vector.ys.GetData(), vector.zs.GetData(), value, vector.num());
}

/* ---------------------------------------------------------------------------------------------- */
// Appending
/* ---------------------------------------------------------------------------------------------- */
inline void append(FVectors3f& vector, float const x, float const y, float const z) {
    vector.xs.Add(x);
    vector.ys.Add(y);
    vector.zs.Add(z);
}
inline void append(FVectors3f& vector, FVector3f const& other) {
    vector.xs.Add(other.X);
    vector.ys.Add(other.Y);
    vector.zs.Add(other.Z);
}
inline void append(FVectors3f& vector, FVector const& other) {
    vector.xs.Add(static_cast<float>(other.X));
    vector.ys.Add(static_cast<float>(other.Y));
    vector.zs.Add(static_cast<float>(other.Z));
}

inline void
    append_n(FVectors3f& vector, int32 const count, float const x, float const y, float const z) {
    auto const offset{ml::num(vector)};

    vector.xs.AddUninitialized(count);
    vector.ys.AddUninitialized(count);
    vector.zs.AddUninitialized(count);

    for (int32 i{0}; i < count; ++i) {
        vector.xs[offset + i] = x;
        vector.ys[offset + i] = y;
        vector.zs[offset + i] = z;
    }
}
inline void append_n(FVectors3f& vector, int32 const count, float const value) {
    auto const offset{ml::num(vector)};

    ml::add_uninitialised(vector, count);

    for (int32 i{0}; i < count; ++i) {
        vector.xs[offset + i] = value;
        vector.ys[offset + i] = value;
        vector.zs[offset + i] = value;
    }
}

template <is_readable_vec3f SrcT>
inline void append_from(FVectors3f& vector, SrcT&& to_append) {
    check(ml::all_pointers_not_equal(vector, to_append));

    auto const n_base{vector.num()};
    auto const n_to_append{to_append.num()};

    ml::add_uninitialised(vector, n_to_append);

    ml::kernel::assign_from(vector.xs.GetData() + n_base,
                            vector.ys.GetData() + n_base,
                            vector.zs.GetData() + n_base,
                            to_append.xs.GetData(),
                            to_append.ys.GetData(),
                            to_append.zs.GetData(),
                            n_to_append);
}

template <>
struct AppendFromTraits<FVectors3f> {
    template <is_readable_vec3f SrcT>
    static void append_from(FVectors3f& dst, SrcT&& src) {
        ml::append_from(dst, src);
    }
};

template <is_vec3f Vec3f>
inline void append_element_from(FVectors3f& vector, Vec3f const& to_append, int32 const i) {
    vector.xs.Add(to_append.xs[i]);
    vector.ys.Add(to_append.ys[i]);
    vector.zs.Add(to_append.zs[i]);
}

/* ---------------------------------------------------------------------------------------------- */
// Addition
/* ---------------------------------------------------------------------------------------------- */
void SANDBOXCORE_API add_scaled_in_place(FVectors3f& dst,
                                         FVectors3f const& src,
                                         float const scale_factor);

void SANDBOXCORE_API add_scaled_in_place(FVectors3f& dst,
                                         FVectors3f const& a,
                                         FVectors3f const& b,
                                         float const c);
template <is_vec3f Dst, is_vec3f Src>
void add_scaled_in_place(Dst& dst, Src const& a, TConstArrayView<float> const b, float const c) {
    auto const n{ml::num(dst)};
    check(ml::all_num_equal_and_pointers_not_equal(dst, a, b));

    if (n < 1) {
        return;
    }

    return ml::kernel::add_scaled_in_place<float>(dst.xs.GetData(),
                                                  dst.ys.GetData(),
                                                  dst.zs.GetData(),
                                                  a.xs.GetData(),
                                                  a.ys.GetData(),
                                                  a.zs.GetData(),
                                                  b.GetData(),
                                                  c,
                                                  n);
}

/* ---------------------------------------------------------------------------------------------- */
// Subtraction
/* ---------------------------------------------------------------------------------------------- */
template <is_mutable_vec3f Dst, is_readable_vec3f A, is_readable_vec3f B>
void subtract_scaled(Dst&& dst, A&& a, B&& b, float const c) {
    auto const n{ml::num(dst)};
    check(ml::all_num_equal_and_pointers_not_equal(dst, a, b));

    if (n < 1) {
        return;
    }

    return ml::kernel::subtract_scaled<float>(dst.xs.GetData(),
                                              dst.ys.GetData(),
                                              dst.zs.GetData(),
                                              a.xs.GetData(),
                                              a.ys.GetData(),
                                              a.zs.GetData(),
                                              b.xs.GetData(),
                                              b.ys.GetData(),
                                              b.zs.GetData(),
                                              c,
                                              n);
}

/* ---------------------------------------------------------------------------------------------- */
// Interpolation
/* ---------------------------------------------------------------------------------------------- */
inline void
    lerp_in_place(FVectors3f& current, FVectors3f const& target, TArrayView<float> const alpha) {
    ml::lerp_3d_in_place(TArrayView<float>{current.xs},
                         TArrayView<float>{current.ys},
                         TArrayView<float>{current.zs},
                         TConstArrayView<float>{target.xs},
                         TConstArrayView<float>{target.ys},
                         TConstArrayView<float>{target.zs},
                         TConstArrayView<float>{alpha.GetData(), alpha.Num()});
}

template <is_vec3f Current, is_vec3f Target>
inline void lerp_in_place(Current& current, Target const& target, float const alpha) {
    ml::lerp_3d_in_place(TArrayView<float>{current.xs},
                         TArrayView<float>{current.ys},
                         TArrayView<float>{current.zs},
                         TConstArrayView<float>{target.xs},
                         TConstArrayView<float>{target.ys},
                         TConstArrayView<float>{target.zs},
                         alpha);
}

/* ---------------------------------------------------------------------------------------------- */
// Multiplication
/* ---------------------------------------------------------------------------------------------- */
template <is_mutable_vec3f Dst, is_readable_vec3f Src>
void multiply(Dst&& dst, Src&& src, TConstArrayView<float> const scale_factors) {
    auto const n{ml::num(dst)};
    check(ml::all_num_equal_and_pointers_not_equal(dst, src, scale_factors));

    if (n < 1) {
        return;
    }

    ml::kernel::multiply<float>(dst.xs.GetData(),
                                dst.ys.GetData(),
                                dst.zs.GetData(),
                                src.xs.GetData(),
                                src.ys.GetData(),
                                src.zs.GetData(),
                                scale_factors.GetData(),
                                n);
}
template <is_mutable_vec3f Dst, is_readable_vec3f Src>
void multiply(Dst&& dst, Src&& src, float const scale_factor) {
    auto const n{ml::num(dst)};
    check(ml::all_num_equal_and_pointers_not_equal(dst, src));

    if (n < 1) {
        return;
    }

    ml::kernel::multiply<float>(dst.xs.GetData(),
                                dst.ys.GetData(),
                                dst.zs.GetData(),
                                src.xs.GetData(),
                                src.ys.GetData(),
                                src.zs.GetData(),
                                scale_factor,
                                n);
}

void SANDBOXCORE_API multiply_in_place(FVectors3f& dst, float const value);
void SANDBOXCORE_API multiply_in_place(FVectors3f& dst, TConstArrayView<float> const values);

/* ---------------------------------------------------------------------------------------------- */
// Size
/* ---------------------------------------------------------------------------------------------- */
template <is_readable_vec3f T>
auto size_sq(T&& vecs, int32 const i) -> float {
    return ml::size_sq(vecs.xs[i], vecs.ys[i], vecs.zs[i]);
}

/* ---------------------------------------------------------------------------------------------- */
// Distance
/* ---------------------------------------------------------------------------------------------- */
template <is_readable_vec3f T>
auto dist_sq(T&& vecs, int32 const i, FVector3f const& other) -> float {
    auto const dx{vecs.xs[i] - other.X};
    auto const dy{vecs.ys[i] - other.Y};
    auto const dz{vecs.zs[i] - other.Z};

    return ml::size_sq(dx, dy, dz);
}
inline auto
    dist_sq(FVectors3f const& vecs, int32 const i, float const ox, float const oy, float const oz)
        -> float {
    auto const dx{vecs.xs[i] - ox};
    auto const dy{vecs.ys[i] - oy};
    auto const dz{vecs.zs[i] - oz};

    return ml::size_sq(dx, dy, dz);
}
template <is_readable_vec3f U, is_readable_vec3f V>
void dist_sq(TArrayView<float> const out, U&& as, V&& bs) {
    check(ml::all_num_equal_and_pointers_not_equal(out, as, bs));

    ml::kernel::dist_sq_vector(out.GetData(),
                               as.xs.GetData(),
                               as.ys.GetData(),
                               as.zs.GetData(),
                               bs.xs.GetData(),
                               bs.ys.GetData(),
                               bs.zs.GetData(),
                               out.Num());
}

template <is_readable_vec3f U, is_readable_vec3f V>
void dist_and_dist_sq(TArrayView<float> const out_distances,
                      TArrayView<float> const out_distances_sq,
                      U&& as,
                      V&& bs) {
    auto const n{ml::num(as)};
    check(out_distances.Num() >= n);
    check(out_distances_sq.Num() >= n);

    auto const distances{out_distances.Left(n)};
    auto const distances_sq{out_distances_sq.Left(n)};
    check(ml::all_num_equal_and_pointers_not_equal(distances, distances_sq, as, bs));

    if (n < 1) {
        return;
    }

    ml::kernel::dist_and_dist_sq_vector(distances.GetData(),
                                        distances_sq.GetData(),
                                        as.xs.GetData(),
                                        as.ys.GetData(),
                                        as.zs.GetData(),
                                        bs.xs.GetData(),
                                        bs.ys.GetData(),
                                        bs.zs.GetData(),
                                        n);
}

/* ---------------------------------------------------------------------------------------------- */
// Direction
/* ---------------------------------------------------------------------------------------------- */
template <is_mutable_vec3f OutType, is_readable_vec3f FromType, is_readable_vec3f ToType>
inline void direction(OutType&& out, FromType&& from, ToType&& to) {
    auto const n{ml::num(from)};
    check(ml::num(out) >= n);
    check(ml::all_num_equal_and_pointers_not_equal(out.get_view(0, n), from, to));

    if (n < 1) {
        return;
    }

    ml::kernel::direction(out.xs.GetData(),
                          out.ys.GetData(),
                          out.zs.GetData(),
                          from.xs.GetData(),
                          from.ys.GetData(),
                          from.zs.GetData(),
                          to.xs.GetData(),
                          to.ys.GetData(),
                          to.zs.GetData(),
                          n);
}

template <is_mutable_vec3f OutType, is_readable_vec3f FromType, is_readable_vec3f ToType>
inline void direction_and_distance(OutType&& out,
                                   TArrayView<float> const out_distances,
                                   FromType&& from,
                                   ToType&& to) {
    auto const n{ml::num(from)};
    check(ml::num(out) >= n);
    check(out_distances.Num() >= n);
    check(ml::all_num_equal_and_pointers_not_equal(
        out.get_view(0, n), out_distances.Left(n), from, to));

    if (n < 1) {
        return;
    }

    ml::kernel::direction_and_distance(out.xs.GetData(),
                                       out.ys.GetData(),
                                       out.zs.GetData(),
                                       out_distances.GetData(),
                                       from.xs.GetData(),
                                       from.ys.GetData(),
                                       from.zs.GetData(),
                                       to.xs.GetData(),
                                       to.ys.GetData(),
                                       to.zs.GetData(),
                                       n);
}

/* ---------------------------------------------------------------------------------------------- */
// Conversion
/* ---------------------------------------------------------------------------------------------- */
template <is_readable_vec3f Vec>
auto get_vector3d(Vec const& src, int32 i) -> FVector {
    return {
        static_cast<double>(src.xs[i]),
        static_cast<double>(src.ys[i]),
        static_cast<double>(src.zs[i]),
    };
}

template <is_readable_vec3f T>
inline auto get_vector3f(T const& src, int32 i) -> FVector3f {
    return {src.xs[i], src.ys[i], src.zs[i]};
}

inline auto scaled_vector3d(FVectors3f const& vectors, int32 const i, float const scale)
    -> FVector {
    return FVector{
        vectors.xs[i] * scale,
        vectors.ys[i] * scale,
        vectors.zs[i] * scale,
    };
}

template <is_vec3f Vec3f>
inline void to_rotatorsf(FRotatorsf& rotators, Vec3f const& vectors) {
    auto const n{vectors.num()};
    rotators.set_num_uninitialised(n);

    ml::to_rotations(TArrayView<float>{rotators.pitches},
                     TArrayView<float>{rotators.yaws},
                     TArrayView<float>{rotators.rolls},
                     TConstArrayView<float>{vectors.xs},
                     TConstArrayView<float>{vectors.ys},
                     TConstArrayView<float>{vectors.zs});
}

template <is_vec3f Vec3f>
inline auto to_rotatorsf(Vec3f const& vectors) -> FRotatorsf {
    FRotatorsf rotators;
    ml::to_rotatorsf(rotators, vectors);

    return rotators;
}

template <is_readable_vec3f T>
inline auto to_vector3f_array(T const& src) -> TArray<FVector3f> {
    TArray<FVector3f> out;

    auto const n{ml::num(src)};
    out.Reserve(n);

    for (int32 i{0}; i < n; ++i) {
        out.Emplace(src.xs[i], src.ys[i], src.zs[i]);
    }

    return out;
}

/* ---------------------------------------------------------------------------------------------- */
// Normalisation
/* ---------------------------------------------------------------------------------------------- */
inline auto all_normalised(FVectors3f::ConstView const vecs) -> bool {
    auto const n{vecs.num()};
    check(ml::all_num_equal_and_pointers_not_equal(vecs));

    for (int32 i{0}; i < n; ++i) {
        auto const size_sq{ml::size_sq(vecs.xs[i], vecs.ys[i], vecs.zs[i])};
        if (!FMath::IsNearlyEqual(size_sq, 1.0f, KINDA_SMALL_NUMBER)) {
            return false;
        }
    }

    return true;
}

/* ---------------------------------------------------------------------------------------------- */
// Dot product
/* ---------------------------------------------------------------------------------------------- */
template <is_readable_vec3f T, is_readable_vec3f U>
void dot_product(TArrayView<float> out, T&& as, U&& bs) noexcept {
    auto const n{ml::num(out)};
    check(ml::all_num_equal_and_pointers_not_equal(out, as, bs));

    ml::kernel::dot_product(out.GetData(),
                            as.xs.GetData(),
                            as.ys.GetData(),
                            as.zs.GetData(),
                            bs.xs.GetData(),
                            bs.ys.GetData(),
                            bs.zs.GetData(),
                            n);
}

}

/* ---------------------------------------------------------------------------------------------- */
// Operators
/* ---------------------------------------------------------------------------------------------- */
inline auto operator*=(FVectors3f& vectors, float const scale) -> FVectors3f& {
    ml::multiply_in_place(vectors, scale);
    return vectors;
}
