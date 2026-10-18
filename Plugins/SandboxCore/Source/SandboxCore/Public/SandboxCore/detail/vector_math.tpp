#pragma once

#include <ioj/vector_math.h>

#include "SandboxCore/array_checks.h"
#include "SandboxCore/array_utils.h"
#include "SandboxCore/log_categories.h"
#include "ioj/numeric.h"
#include "SandboxCore/vector_concepts.h"
#include "SandboxCore/vector_traits.h"

#include "CoreMinimal.h"

namespace ioj {
/* ---------------------------------------------------------------------------------------------- */
// Size
/* ---------------------------------------------------------------------------------------------- */
template <ioj::Numeric T>
FORCEINLINE auto size_sq(T const x, T const y, T const z) noexcept -> T {
    return ioj::native_math::size_squared(x, y, z);
}
template <ioj::Numeric T>
auto size(T const x, T const y, T const z) noexcept -> T {
    return ioj::native_math::size(x, y, z);
}

/* ---------------------------------------------------------------------------------------------- */
// Distance
/* ---------------------------------------------------------------------------------------------- */
template <ioj::Numeric T>
FORCEINLINE auto
    dist_sq(T const ax, T const ay, T const az, T const bx, T const by, T const bz) noexcept -> T {
    return ioj::native_math::distance_squared(ax, ay, az, bx, by, bz);
}
template <ioj::Numeric T>
auto dist(T const ax, T const ay, T const az, T const bx, T const by, T const bz) noexcept -> T {
    return ioj::native_math::distance(ax, ay, az, bx, by, bz);
}

/* ---------------------------------------------------------------------------------------------- */
// Dot product
/* ---------------------------------------------------------------------------------------------- */
template <ioj::Numeric T>
auto dot_product(T const ax, T const ay, T const az, T const bx, T const by, T const bz) noexcept
    -> T {
    return ioj::native_math::dot_product(ax, ay, az, bx, by, bz);
}

/* ---------------------------------------------------------------------------------------------- */
// Rotation
/* ---------------------------------------------------------------------------------------------- */
template <std::floating_point T>
auto to_rotation(T const x, T const y, T const z) noexcept -> UE::Math::TRotator<T> {
    auto const yaw{FMath::RadiansToDegrees(FMath::Atan2(y, x))};
    auto const pitch{FMath::RadiansToDegrees(FMath::Atan2(z, FMath::Sqrt((x * x) + (y * y))))};

    return {pitch, yaw, static_cast<T>(0.0)};
}
}

namespace ioj::kernel {
/* ---------------------------------------------------------------------------------------------- */
// Addition
/* ---------------------------------------------------------------------------------------------- */
template <ioj::Numeric T>
void add_vector3(T* RESTRICT out_x,
                 T* RESTRICT out_y,
                 T* RESTRICT out_z,
                 T const* RESTRICT lhs_x,
                 T const* RESTRICT lhs_y,
                 T const* RESTRICT lhs_z,
                 T const* RESTRICT rhs_x,
                 T const* RESTRICT rhs_y,
                 T const* RESTRICT rhs_z,
                 int32 const count) noexcept {
    ioj::native_math::add_vector3(
        out_x, out_y, out_z, lhs_x, lhs_y, lhs_z, rhs_x, rhs_y, rhs_z, count);
}

template <ioj::Numeric T>
void add_vector3_in_place(T* dst_x,
                          T* dst_y,
                          T* dst_z,
                          T const* RESTRICT src_x,
                          T const* RESTRICT src_y,
                          T const* RESTRICT src_z,
                          int32 const count) noexcept {
    ioj::native_math::add_vector3_in_place(dst_x, dst_y, dst_z, src_x, src_y, src_z, count);
}

template <ioj::Numeric T>
void add_scaled_in_place(T* const RESTRICT dst_x,
                         T* const RESTRICT dst_y,
                         T* const RESTRICT dst_z,
                         T const* const RESTRICT src_x,
                         T const* const RESTRICT src_y,
                         T const* const RESTRICT src_z,
                         T const scale_factor,
                         int32 const count) {
    ioj::native_math::add_scaled_in_place(
        dst_x, dst_y, dst_z, src_x, src_y, src_z, scale_factor, count);
}

template <ioj::Numeric T>
void add_scaled_in_place(T* const RESTRICT dst_x,
                         T* const RESTRICT dst_y,
                         T* const RESTRICT dst_z,
                         T const* const RESTRICT a_x,
                         T const* const RESTRICT a_y,
                         T const* const RESTRICT a_z,
                         T const* const RESTRICT b_x,
                         T const* const RESTRICT b_y,
                         T const* const RESTRICT b_z,
                         T const c,
                         int32 const count) {
    ioj::native_math::add_scaled_product_in_place(
        dst_x, dst_y, dst_z, a_x, a_y, a_z, b_x, b_y, b_z, c, count);
}

template <ioj::Numeric T>
void add_scaled_in_place(T* const RESTRICT dst_x,
                         T* const RESTRICT dst_y,
                         T* const RESTRICT dst_z,
                         T const* const RESTRICT a_x,
                         T const* const RESTRICT a_y,
                         T const* const RESTRICT a_z,
                         T const* const RESTRICT b,
                         T const c,
                         int32 const count) {
    ioj::native_math::add_scaled_product_in_place(
        dst_x, dst_y, dst_z, a_x, a_y, a_z, b, c, count);
}

/* ---------------------------------------------------------------------------------------------- */
// Subtraction
/* ---------------------------------------------------------------------------------------------- */
template <ioj::Numeric T>
void subtract_scaled(T* RESTRICT out_x,
                     T* RESTRICT out_y,
                     T* RESTRICT out_z,
                     T const* RESTRICT a_x,
                     T const* RESTRICT a_y,
                     T const* RESTRICT a_z,
                     T const* RESTRICT b_x,
                     T const* RESTRICT b_y,
                     T const* RESTRICT b_z,
                     T const c,
                     int32 const count) noexcept {
    ioj::native_math::subtract_scaled(
        out_x, out_y, out_z, a_x, a_y, a_z, b_x, b_y, b_z, c, count);
}
template <ioj::Numeric T>
void subtract_scaled_in_place(T* RESTRICT a_x,
                              T* RESTRICT a_y,
                              T* RESTRICT a_z,
                              T const* RESTRICT b_x,
                              T const* RESTRICT b_y,
                              T const* RESTRICT b_z,
                              T const c,
                              int32 const count) noexcept {
    ioj::native_math::subtract_scaled_in_place(a_x, a_y, a_z, b_x, b_y, b_z, c, count);
}

/* ---------------------------------------------------------------------------------------------- */
// Multiplication
/* ---------------------------------------------------------------------------------------------- */
template <ioj::Numeric T>
void multiply_in_place(
    T* RESTRICT a_x, T* RESTRICT a_y, T* RESTRICT a_z, T const b, int32 const count) noexcept {
    ioj::native_math::multiply_vector3(a_x, a_y, a_z, a_x, a_y, a_z, b, count);
}

template <ioj::Numeric T>
void multiply_in_place(T* RESTRICT a_x,
                       T* RESTRICT a_y,
                       T* RESTRICT a_z,
                       T const* RESTRICT b,
                       int32 const count) noexcept {
    ioj::native_math::multiply_vector3(a_x, a_y, a_z, a_x, a_y, a_z, b, count);
}

template <ioj::Numeric T>
void multiply_in_place(T* RESTRICT a_x,
                       T* RESTRICT a_y,
                       T* RESTRICT a_z,
                       T const* RESTRICT b_x,
                       T const* RESTRICT b_y,
                       T const* RESTRICT b_z,
                       int32 const count) noexcept {
    ioj::native_math::multiply_vector3_in_place(a_x, a_y, a_z, b_x, b_y, b_z, count);
}

template <ioj::Numeric T>
void multiply(T* RESTRICT dst_x,
              T* RESTRICT dst_y,
              T* RESTRICT dst_z,
              T const* RESTRICT lhs_x,
              T const* RESTRICT lhs_y,
              T const* RESTRICT lhs_z,
              T const* RESTRICT scale_factor,
              int32 const count) noexcept {
    ioj::native_math::multiply_vector3(
        dst_x, dst_y, dst_z, lhs_x, lhs_y, lhs_z, scale_factor, count);
}
template <ioj::Numeric T>
void multiply(T* RESTRICT dst_x,
              T* RESTRICT dst_y,
              T* RESTRICT dst_z,
              T const* RESTRICT lhs_x,
              T const* RESTRICT lhs_y,
              T const* RESTRICT lhs_z,
              T const scale_factor,
              int32 const count) noexcept {
    ioj::native_math::multiply_vector3(
        dst_x, dst_y, dst_z, lhs_x, lhs_y, lhs_z, scale_factor, count);
}

/* ---------------------------------------------------------------------------------------------- */
// Size
/* ---------------------------------------------------------------------------------------------- */
template <ioj::HasSizeSquared T>
void size_sq_vector(VectorElementT<T>* RESTRICT out,
                    T const* RESTRICT vecs,
                    int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = vecs[i].SizeSquared();
    }
}

template <ioj::Numeric T>
void size_sq_vector(T* RESTRICT out,
                    T const* RESTRICT xs,
                    T const* RESTRICT ys,
                    T const* RESTRICT zs,
                    int32 const count) noexcept {
    ioj::native_math::size_squared_vector(out, xs, ys, zs, count);
}

/* ---------------------------------------------------------------------------------------------- */
// Distance
/* ---------------------------------------------------------------------------------------------- */
template <ioj::HasSizeSquared T>
void dist_sq_vector(VectorElementT<T>* RESTRICT out,
                    T const reference,
                    T const* RESTRICT points,
                    int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = (points[i] - reference).SizeSquared();
    }
}

template <ioj::Numeric T>
void dist_sq_vector(T* RESTRICT out,
                    T const* RESTRICT xs_lhs,
                    T const* RESTRICT ys_lhs,
                    T const* RESTRICT zs_lhs,
                    T const* RESTRICT xs_rhs,
                    T const* RESTRICT ys_rhs,
                    T const* RESTRICT zs_rhs,
                    int32 const count) noexcept {
    ioj::native_math::distance_squared_vector(
        out, xs_lhs, ys_lhs, zs_lhs, xs_rhs, ys_rhs, zs_rhs, count);
}

template <std::floating_point T>
void dist_and_dist_sq_vector(T* RESTRICT out_distances,
                             T* RESTRICT out_distances_sq,
                             T const* RESTRICT xs_lhs,
                             T const* RESTRICT ys_lhs,
                             T const* RESTRICT zs_lhs,
                             T const* RESTRICT xs_rhs,
                             T const* RESTRICT ys_rhs,
                             T const* RESTRICT zs_rhs,
                             int32 const count) noexcept {
    ioj::native_math::distance_and_squared_vector(out_distances,
                                                  out_distances_sq,
                                                  xs_lhs,
                                                  ys_lhs,
                                                  zs_lhs,
                                                  xs_rhs,
                                                  ys_rhs,
                                                  zs_rhs,
                                                  count);
}

/* ---------------------------------------------------------------------------------------------- */
// Dot product
/* ---------------------------------------------------------------------------------------------- */
template <ioj::Numeric T>
void dot_product(T* RESTRICT out,
                 T const* RESTRICT a_xs,
                 T const* RESTRICT a_ys,
                 T const* RESTRICT a_zs,
                 T const* RESTRICT b_xs,
                 T const* RESTRICT b_ys,
                 T const* RESTRICT b_zs,
                 int32 const count) noexcept {
    ioj::native_math::dot_product_vector(
        out, a_xs, a_ys, a_zs, b_xs, b_ys, b_zs, count);
}

/* ---------------------------------------------------------------------------------------------- */
// Direction
/* ---------------------------------------------------------------------------------------------- */
template <std::floating_point T>
void direction(T* const RESTRICT out_xs,
               T* const RESTRICT out_ys,
               T* const RESTRICT out_zs,
               T const* const RESTRICT from_xs,
               T const* const RESTRICT from_ys,
               T const* const RESTRICT from_zs,
               T const* const RESTRICT to_xs,
               T const* const RESTRICT to_ys,
               T const* const RESTRICT to_zs,
               int32 const count) noexcept {
    ioj::native_math::direction_and_distance(out_xs,
                                            out_ys,
                                            out_zs,
                                            static_cast<T*>(nullptr),
                                            from_xs,
                                            from_ys,
                                            from_zs,
                                            to_xs,
                                            to_ys,
                                            to_zs,
                                            count);
}

template <std::floating_point T>
void direction_and_distance(T* const RESTRICT out_xs,
                            T* const RESTRICT out_ys,
                            T* const RESTRICT out_zs,
                            T* const RESTRICT out_distances,
                            T const* const RESTRICT from_xs,
                            T const* const RESTRICT from_ys,
                            T const* const RESTRICT from_zs,
                            T const* const RESTRICT to_xs,
                            T const* const RESTRICT to_ys,
                            T const* const RESTRICT to_zs,
                            int32 const count) noexcept {
    ioj::native_math::direction_and_distance(out_xs,
                                            out_ys,
                                            out_zs,
                                            out_distances,
                                            from_xs,
                                            from_ys,
                                            from_zs,
                                            to_xs,
                                            to_ys,
                                            to_zs,
                                            count);
}

/* ---------------------------------------------------------------------------------------------- */
// Rotation
/* ---------------------------------------------------------------------------------------------- */
template <std::floating_point T>
void to_rotations(T* const RESTRICT pitches,
                  T* const RESTRICT yaws,
                  T* const RESTRICT rolls,
                  T const* const RESTRICT xs,
                  T const* const RESTRICT ys,
                  T const* const RESTRICT zs,
                  int32 const count) noexcept {
    ioj::native_math::to_rotations(pitches, yaws, rolls, xs, ys, zs, count);
}
}

namespace ioj {
/* ---------------------------------------------------------------------------------------------- */
// Addition
/* ---------------------------------------------------------------------------------------------- */
template <ioj::Numeric T>
void add_vector3(TArrayView<T> out_x,
                 TArrayView<T> out_y,
                 TArrayView<T> out_z,
                 TConstArrayView<T> lhs_x,
                 TConstArrayView<T> lhs_y,
                 TConstArrayView<T> lhs_z,
                 TConstArrayView<T> rhs_x,
                 TConstArrayView<T> rhs_y,
                 TConstArrayView<T> rhs_z) noexcept {
    auto const count{lhs_x.Num()};
    check(ioj::all_num_equal_and_pointers_not_equal(
        out_x, out_y, out_z, lhs_x, lhs_y, lhs_z, rhs_x, rhs_y, rhs_z));

    ioj::kernel::add_vector3(out_x.GetData(),
                            out_y.GetData(),
                            out_z.GetData(),
                            lhs_x.GetData(),
                            lhs_y.GetData(),
                            lhs_z.GetData(),
                            rhs_x.GetData(),
                            rhs_y.GetData(),
                            rhs_z.GetData(),
                            count);
}

/* ---------------------------------------------------------------------------------------------- */
// Size
/* ---------------------------------------------------------------------------------------------- */
template <ioj::HasSizeSquared T>
void size_sq_vector(TConstArrayView<T> vecs, TArrayView<T> out) noexcept {
    check(ioj::all_num_equal_and_pointers_not_equal(vecs, out));
    size_sq_vector(out.GetData(), vecs.GetData(), vecs.Num());
}

template <ioj::Numeric T>
void size_sq_vector(TArrayView<T> out,
                    TConstArrayView<T> xs,
                    TConstArrayView<T> ys,
                    TConstArrayView<T> zs) noexcept {
    auto const count{xs.Num()};

    auto const inputs_are_valid{ioj::all_num_equal_and_pointers_not_equal(out, xs, ys, zs)};
    check(inputs_are_valid);
    if (!inputs_are_valid) {
        UE_LOG(LogSandboxCore, Error, TEXT("size_sq_vector: Invalid array sizes or aliases."));
        checkNoEntry();
        return;
    }
    if (count < 1) {
        return;
    }
    size_sq_vector(out.GetData(), xs.GetData(), ys.GetData(), zs.GetData(), count);
}

/* ---------------------------------------------------------------------------------------------- */
// Dot product
/* ---------------------------------------------------------------------------------------------- */
template <ioj::Numeric T>
void dot_product(TArrayView<T> const out,
                 TConstArrayView<T> const a_xs,
                 TConstArrayView<T> const a_ys,
                 TConstArrayView<T> const a_zs,
                 TConstArrayView<T> const b_xs,
                 TConstArrayView<T> const b_ys,
                 TConstArrayView<T> const b_zs) noexcept {
    auto const n{a_xs.Num()};
    check(ioj::all_num_equal_and_pointers_not_equal(
        out, a_xs, a_ys, a_zs, b_xs, b_ys, b_zs));

    ioj::kernel::dot_product(out.GetData(),
                            a_xs.GetData(),
                            a_ys.GetData(),
                            a_zs.GetData(),
                            b_xs.GetData(),
                            b_ys.GetData(),
                            b_zs.GetData(),
                            n);
}

/* ---------------------------------------------------------------------------------------------- */
// Direction
/* ---------------------------------------------------------------------------------------------- */
template <std::floating_point T>
void direction(TArrayView<T> const out_xs,
               TArrayView<T> const out_ys,
               TArrayView<T> const out_zs,
               TConstArrayView<T> const a_xs,
               TConstArrayView<T> const a_ys,
               TConstArrayView<T> const a_zs,
               TConstArrayView<T> const b_xs,
               TConstArrayView<T> const b_ys,
               TConstArrayView<T> const b_zs) noexcept {
    auto const count{out_xs.Num()};

    check(ioj::all_num_equal_and_pointers_not_equal(
        out_xs, out_ys, out_zs, a_xs, a_ys, a_zs, b_xs, b_ys, b_zs));

    ioj::kernel::direction(out_xs.GetData(),
                          out_ys.GetData(),
                          out_zs.GetData(),
                          a_xs.GetData(),
                          a_ys.GetData(),
                          a_zs.GetData(),
                          b_xs.GetData(),
                          b_ys.GetData(),
                          b_zs.GetData(),
                          count);
}

/* ---------------------------------------------------------------------------------------------- */
// Rotation
/* ---------------------------------------------------------------------------------------------- */
template <std::floating_point T>
void to_rotations(TArrayView<T> const pitches,
                  TArrayView<T> const yaws,
                  TArrayView<T> const rolls,
                  TConstArrayView<T> const xs,
                  TConstArrayView<T> const ys,
                  TConstArrayView<T> const zs) noexcept {
    auto const count{xs.Num()};

    check(ioj::all_num_equal_and_pointers_not_equal(pitches, yaws, rolls, xs, ys, zs));

    ioj::kernel::to_rotations(pitches.GetData(),
                             yaws.GetData(),
                             rolls.GetData(),
                             xs.GetData(),
                             ys.GetData(),
                             zs.GetData(),
                             count);
}
}
