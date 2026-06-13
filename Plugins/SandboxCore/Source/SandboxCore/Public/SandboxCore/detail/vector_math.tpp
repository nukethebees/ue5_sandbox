#pragma once

#include "SandboxCore/array_utils.h"
#include "SandboxCore/log_categories.h"
#include "SandboxCore/numeric.h"
#include "SandboxCore/vector_concepts.h"
#include "SandboxCore/vector_traits.h"

#include "CoreMinimal.h"

namespace ml {
template <ml::Numeric T>
auto size_squared(T const x, T const y, T const z) noexcept -> T {
    return x * x + y * y + z * z;
}
template <ml::Numeric T>
auto dist_squared(T const x0, T const y0, T const z0, T const x1, T const y1, T const z1) noexcept
    -> T {
    auto const dx{x1 - x0};
    auto const dy{y1 - y0};
    auto const dz{z1 - z0};

    return size_squared(dx, dy, dz);
}
}

namespace ml::kernel {
// -------------------------------------------------------------------------------------------------
// Addition
// -------------------------------------------------------------------------------------------------
template <ml::Numeric T>
void add_vector3(T const* RESTRICT lhs_x,
                 T const* RESTRICT lhs_y,
                 T const* RESTRICT lhs_z,
                 T const* RESTRICT rhs_x,
                 T const* RESTRICT rhs_y,
                 T const* RESTRICT rhs_z,
                 T* RESTRICT out_x,
                 T* RESTRICT out_y,
                 T* RESTRICT out_z,
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
void size_squared_vector(T const* RESTRICT vecs,
                         int32 const count,
                         VectorElementT<T>* RESTRICT out) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = vecs[i].SizeSquared();
    }
}

template <ml::Numeric T>
void size_squared_vector(T const* RESTRICT xs,
                         T const* RESTRICT ys,
                         T const* RESTRICT zs,
                         int32 const count,
                         T* RESTRICT out) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = xs[i] * xs[i] + ys[i] * ys[i] + zs[i] * zs[i];
    }
}

// -------------------------------------------------------------------------------------------------
// Distance
// -------------------------------------------------------------------------------------------------
template <ml::HasSizeSquared T>
void dist_squared_vector(T const reference,
                         T const* RESTRICT points,
                         int32 const count,
                         VectorElementT<T>* RESTRICT out) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = (points[i] - reference).SizeSquared();
    }
}

template <ml::Numeric T>
void dist_squared_vector(T const* RESTRICT xs_lhs,
                         T const* RESTRICT ys_lhs,
                         T const* RESTRICT zs_lhs,
                         T const* RESTRICT xs_rhs,
                         T const* RESTRICT ys_rhs,
                         T const* RESTRICT zs_rhs,
                         int32 const count,
                         T* RESTRICT out) noexcept {
    for (int32 i{0}; i < count; ++i) {
        auto const x{xs_rhs[i] - xs_lhs[i]};
        auto const y{ys_rhs[i] - ys_lhs[i]};
        auto const z{zs_rhs[i] - zs_lhs[i]};
        out[i] = x * x + y * y + z * z;
    }
}
}

namespace ml {

template <ml::Numeric T>
bool add_vector3(TConstArrayView<T> lhs_x,
                 TConstArrayView<T> lhs_y,
                 TConstArrayView<T> lhs_z,
                 TConstArrayView<T> rhs_x,
                 TConstArrayView<T> rhs_y,
                 TConstArrayView<T> rhs_z,
                 TArrayView<T> out_x,
                 TArrayView<T> out_y,
                 TArrayView<T> out_z) noexcept {
    auto const count{lhs_x.Num()};

    if (!ml::all_num_equal_to(count, lhs_y, lhs_z, rhs_x, rhs_y, rhs_z, out_x, out_y, out_z)) {
        return false;
    }

    ml::kernel::add_vector3(lhs_x.GetData(),
                            lhs_y.GetData(),
                            lhs_z.GetData(),
                            rhs_x.GetData(),
                            rhs_y.GetData(),
                            rhs_z.GetData(),
                            out_x.GetData(),
                            out_y.GetData(),
                            out_z.GetData(),
                            count);

    return true;
}

template <ml::HasSizeSquared T>
void size_squared_vector(TConstArrayView<T> vecs, TArrayView<T> out) noexcept {
    check(vecs.Num() == out.Num());
    size_squared_vector(vecs.GetData(), vecs.Num(), out.GetData());
}

template <ml::Numeric T>
void size_squared_vector(TConstArrayView<T> xs,
                         TConstArrayView<T> ys,
                         TConstArrayView<T> zs,
                         TArrayView<T> out) noexcept {
    auto const count{xs.Num()};

    auto const are_equal{ml::all_num_equal_to(count, ys, zs, out)};
    check(are_equal);
    if (!are_equal) {
        UE_LOG(LogSandboxCore, Error, TEXT("size_squared_vector: Mismatched array sizes."));
        checkNoEntry();
        return;
    }

    size_squared_vector(xs.GetData(), ys.GetData(), zs.GetData(), count, out.GetData());
}
}
