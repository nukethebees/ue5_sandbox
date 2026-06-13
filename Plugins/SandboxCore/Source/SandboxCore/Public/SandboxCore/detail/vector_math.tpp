#pragma once

#include "SandboxCore/array_utils.h"
#include "SandboxCore/log_categories.h"
#include "SandboxCore/numeric.h"
#include "SandboxCore/vector_concepts.h"
#include "SandboxCore/vector_traits.h"

#include "CoreMinimal.h"

namespace ml {
// -------------------------------------------------------------------------------------------------
// Size
// -------------------------------------------------------------------------------------------------
template <ml::Numeric T>
FORCEINLINE auto size_sq(T const x, T const y, T const z) noexcept -> T {
    return x * x + y * y + z * z;
}
template <ml::Numeric T>
auto size(T const x, T const y, T const z) noexcept -> T {
    return FMath::Sqrt(size_sq(x, y, z));
}

// -------------------------------------------------------------------------------------------------
// Distance
// -------------------------------------------------------------------------------------------------
template <ml::Numeric T>
FORCEINLINE auto
    dist_sq(T const ax, T const ay, T const az, T const bx, T const by, T const bz) noexcept -> T {
    auto const dx{bx - ax};
    auto const dy{by - ay};
    auto const dz{bz - az};

    return size_sq(dx, dy, dz);
}
template <ml::Numeric T>
auto dist(T const ax, T const ay, T const az, T const bx, T const by, T const bz) noexcept -> T {
    return FMath::Sqrt(dist_sq(ax, ay, az, bx, by, bz));
}

// -------------------------------------------------------------------------------------------------
// Dot product
// -------------------------------------------------------------------------------------------------
template <ml::Numeric T>
auto dot_product(T const ax, T const ay, T const az, T const bx, T const by, T const bz) noexcept
    -> T {
    return (ax * bx) + (ay * by) + (az * bz);
}

// -------------------------------------------------------------------------------------------------
// Rotation
// -------------------------------------------------------------------------------------------------
template <std::floating_point T>
auto to_rotation(T const x, T const y, T const z) noexcept -> UE::Math::TRotator<T> {
    auto const yaw{FMath::RadiansToDegrees(FMath::Atan2(y, x))};
    auto const pitch{FMath::RadiansToDegrees(FMath::Atan2(z, FMath::Sqrt((x * x) + (y * y))))};

    return {pitch, yaw, static_cast<T>(0.0)};
}
}

namespace ml::kernel {
// -------------------------------------------------------------------------------------------------
// Addition
// -------------------------------------------------------------------------------------------------
template <ml::Numeric T>
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
    for (int32 i{0}; i < count; ++i) {
        out_x[i] = lhs_x[i] + rhs_x[i];
        out_y[i] = lhs_y[i] + rhs_y[i];
        out_z[i] = lhs_z[i] + rhs_z[i];
    }
}

template <ml::Numeric T>
void add_vector3_in_place(T* dst_x,
                          T* dst_y,
                          T* dst_z,
                          T const* RESTRICT src_x,
                          T const* RESTRICT src_y,
                          T const* RESTRICT src_z,
                          int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        dst_x[i] += src_x[i];
        dst_y[i] += src_y[i];
        dst_z[i] += src_z[i];
    }
}

template <ml::Numeric T>
void add_scaled_in_place(T* const RESTRICT dst_x,
                         T* const RESTRICT dst_y,
                         T* const RESTRICT dst_z,
                         T const* const RESTRICT src_x,
                         T const* const RESTRICT src_y,
                         T const* const RESTRICT src_z,
                         T const scale_factor,
                         int32 const count) {
    for (int32 i{0}; i < count; ++i) {
        dst_x[i] += src_x[i] * scale_factor;
        dst_y[i] += src_y[i] * scale_factor;
        dst_z[i] += src_z[i] * scale_factor;
    }
}

// -------------------------------------------------------------------------------------------------
// Multiplication
// -------------------------------------------------------------------------------------------------
template <ml::Numeric T>
void scale_vector3(T* RESTRICT dst_x,
                   T* RESTRICT dst_y,
                   T* RESTRICT dst_z,
                   T const* RESTRICT lhs_x,
                   T const* RESTRICT lhs_y,
                   T const* RESTRICT lhs_z,
                   T const* RESTRICT scale_factor,
                   int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        dst_x[i] = lhs_x[i] * scale_factor[i];
        dst_y[i] = lhs_y[i] * scale_factor[i];
        dst_z[i] = lhs_z[i] * scale_factor[i];
    }
}

// -------------------------------------------------------------------------------------------------
// Size
// -------------------------------------------------------------------------------------------------
template <ml::HasSizeSquared T>
void size_sq_vector(VectorElementT<T>* RESTRICT out,
                    T const* RESTRICT vecs,
                    int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = vecs[i].SizeSquared();
    }
}

template <ml::Numeric T>
void size_sq_vector(T* RESTRICT out,
                    T const* RESTRICT xs,
                    T const* RESTRICT ys,
                    T const* RESTRICT zs,
                    int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = xs[i] * xs[i] + ys[i] * ys[i] + zs[i] * zs[i];
    }
}

// -------------------------------------------------------------------------------------------------
// Distance
// -------------------------------------------------------------------------------------------------
template <ml::HasSizeSquared T>
void dist_sq_vector(VectorElementT<T>* RESTRICT out,
                    T const reference,
                    T const* RESTRICT points,
                    int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = (points[i] - reference).SizeSquared();
    }
}

template <ml::Numeric T>
void dist_sq_vector(T* RESTRICT out,
                    T const* RESTRICT xs_lhs,
                    T const* RESTRICT ys_lhs,
                    T const* RESTRICT zs_lhs,
                    T const* RESTRICT xs_rhs,
                    T const* RESTRICT ys_rhs,
                    T const* RESTRICT zs_rhs,
                    int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        auto const x{xs_rhs[i] - xs_lhs[i]};
        auto const y{ys_rhs[i] - ys_lhs[i]};
        auto const z{zs_rhs[i] - zs_lhs[i]};
        out[i] = x * x + y * y + z * z;
    }
}

// -------------------------------------------------------------------------------------------------
// Dot product
// -------------------------------------------------------------------------------------------------
template <ml::Numeric T>
void dot_product(T* RESTRICT out,
                 T const* RESTRICT a_xs,
                 T const* RESTRICT a_ys,
                 T const* RESTRICT a_zs,
                 T const* RESTRICT b_xs,
                 T const* RESTRICT b_ys,
                 T const* RESTRICT b_zs,
                 int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = (a_xs[i] * b_xs[i]) + (a_ys[i] * b_ys[i]) + (a_zs[i] * b_zs[i]);
    }
}

// -------------------------------------------------------------------------------------------------
// Direction
// -------------------------------------------------------------------------------------------------
template <std::floating_point T>
void direction(T* const RESTRICT out_xs,
               T* const RESTRICT out_ys,
               T* const RESTRICT out_zs,
               T const* const RESTRICT a_xs,
               T const* const RESTRICT a_ys,
               T const* const RESTRICT a_zs,
               T const* const RESTRICT b_xs,
               T const* const RESTRICT b_ys,
               T const* const RESTRICT b_zs,
               int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        auto const dx{b_xs[i] - a_xs[i]};
        auto const dy{b_ys[i] - a_ys[i]};
        auto const dz{b_zs[i] - a_zs[i]};

        auto const size_sq{ml::size_sq(dx, dy, dz)};

        if (size_sq <= static_cast<T>(UE_SMALL_NUMBER)) {
            out_xs[i] = T{0};
            out_ys[i] = T{0};
            out_zs[i] = T{0};
            continue;
        }

        auto const inv_size{static_cast<T>(1) / FMath::Sqrt(size_sq)};

        out_xs[i] = dx * inv_size;
        out_ys[i] = dy * inv_size;
        out_zs[i] = dz * inv_size;
    }
}

// -------------------------------------------------------------------------------------------------
// Rotation
// -------------------------------------------------------------------------------------------------
template <std::floating_point T>
void to_rotations(T* const RESTRICT pitches,
                  T* const RESTRICT yaws,
                  T* const RESTRICT rolls,
                  T const* const RESTRICT xs,
                  T const* const RESTRICT ys,
                  T const* const RESTRICT zs,
                  int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        auto const rotation{ml::to_rotation(xs[i], ys[i], zs[i])};
        pitches[i] = rotation.Pitch;
        yaws[i] = rotation.Yaw;
        rolls[i] = rotation.Roll;
    }
}
}

namespace ml {
// -------------------------------------------------------------------------------------------------
// Addition
// -------------------------------------------------------------------------------------------------
template <ml::Numeric T>
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
    check(ml::all_num_equal_to(count, lhs_y, lhs_z, rhs_x, rhs_y, rhs_z, out_x, out_y, out_z));

    ml::kernel::add_vector3(out_x.GetData(),
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

// -------------------------------------------------------------------------------------------------
// Size
// -------------------------------------------------------------------------------------------------
template <ml::HasSizeSquared T>
void size_sq_vector(TConstArrayView<T> vecs, TArrayView<T> out) noexcept {
    check(vecs.Num() == out.Num());
    size_sq_vector(out.GetData(), vecs.GetData(), vecs.Num());
}

template <ml::Numeric T>
void size_sq_vector(TArrayView<T> out,
                    TConstArrayView<T> xs,
                    TConstArrayView<T> ys,
                    TConstArrayView<T> zs) noexcept {
    auto const count{xs.Num()};

    auto const are_equal{ml::all_num_equal_to(count, ys, zs, out)};
    check(are_equal);
    if (!are_equal) {
        UE_LOG(LogSandboxCore, Error, TEXT("size_sq_vector: Mismatched array sizes."));
        checkNoEntry();
        return;
    }

    size_sq_vector(out.GetData(), xs.GetData(), ys.GetData(), zs.GetData(), count);
}

// -------------------------------------------------------------------------------------------------
// Dot product
// -------------------------------------------------------------------------------------------------
template <ml::Numeric T>
void dot_product(TArrayView<T> const out,
                 TConstArrayView<T> const a_xs,
                 TConstArrayView<T> const a_ys,
                 TConstArrayView<T> const a_zs,
                 TConstArrayView<T> const b_xs,
                 TConstArrayView<T> const b_ys,
                 TConstArrayView<T> const b_zs) noexcept {
    auto const n{a_xs.Num()};

    check(n == a_xs.Num());
    check(n == a_ys.Num());
    check(n == a_zs.Num());
    check(n == b_xs.Num());
    check(n == b_ys.Num());
    check(n == b_zs.Num());
    check(n == out.Num());

    ml::kernel::dot_product(out.GetData(),
                            a_xs.GetData(),
                            a_ys.GetData(),
                            a_zs.GetData(),
                            b_xs.GetData(),
                            b_ys.GetData(),
                            b_zs.GetData(),
                            n);
}

// -------------------------------------------------------------------------------------------------
// Direction
// -------------------------------------------------------------------------------------------------
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

    check(count == a_xs.Num());
    check(count == a_ys.Num());
    check(count == a_zs.Num());
    check(count == b_xs.Num());
    check(count == b_ys.Num());
    check(count == b_zs.Num());
    check(count == out_xs.Num());
    check(count == out_ys.Num());
    check(count == out_zs.Num());

    ml::kernel::direction(out_xs.GetData(),
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

// -------------------------------------------------------------------------------------------------
// Rotation
// -------------------------------------------------------------------------------------------------
template <std::floating_point T>
void to_rotations(TArrayView<T> const pitches,
                  TArrayView<T> const yaws,
                  TArrayView<T> const rolls,
                  TConstArrayView<T> const xs,
                  TConstArrayView<T> const ys,
                  TConstArrayView<T> const zs) noexcept {
    auto const count{xs.Num()};

    check(count == ys.Num());
    check(count == zs.Num());
    check(count == pitches.Num());
    check(count == yaws.Num());
    check(count == rolls.Num());

    ml::kernel::to_rotations(pitches.GetData(),
                             yaws.GetData(),
                             rolls.GetData(),
                             xs.GetData(),
                             ys.GetData(),
                             zs.GetData(),
                             count);
}
}
