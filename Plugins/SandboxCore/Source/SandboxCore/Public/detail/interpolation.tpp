#pragma once

#include "array_utils.h"
#include "numeric.h"

#include "CoreMinimal.h"

namespace ml::kernel {
template <Numeric T>
void lerp_1d(T const* RESTRICT from,
             T const* RESTRICT to,
             T const* RESTRICT alpha,
             T* RESTRICT out,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = from[i] + alpha[i] * (to[i] - from[i]);
    }
}

#define ML_EXTERN_FN(T)                                                      \
    extern template SANDBOXCORE_API void lerp_1d<T>(T const* RESTRICT from,  \
                                                    T const* RESTRICT to,    \
                                                    T const* RESTRICT alpha, \
                                                    T* RESTRICT out,         \
                                                    int32 const count) noexcept
ML_EXTERN_FN(float);
ML_EXTERN_FN(double);
#undef ML_EXTERN_FN

template <Numeric T>
void lerp_1d(T const* RESTRICT from,
             T const* RESTRICT to,
             T alpha,
             T* RESTRICT out,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = from[i] + alpha * (to[i] - from[i]);
    }
}

template <Numeric T>
void lerp_2d(T const* RESTRICT from_x,
             T const* RESTRICT from_y,
             T const* RESTRICT to_x,
             T const* RESTRICT to_y,
             T const* RESTRICT alpha,
             T* RESTRICT out_x,
             T* RESTRICT out_y,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out_x[i] = from_x[i] + alpha[i] * (to_x[i] - from_x[i]);
        out_y[i] = from_y[i] + alpha[i] * (to_y[i] - from_y[i]);
    }
}

template <Numeric T>
void lerp_2d(T const* RESTRICT from_x,
             T const* RESTRICT from_y,
             T const* RESTRICT to_x,
             T const* RESTRICT to_y,
             T const alpha,
             T* RESTRICT out_x,
             T* RESTRICT out_y,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out_x[i] = from_x[i] + alpha * (to_x[i] - from_x[i]);
        out_y[i] = from_y[i] + alpha * (to_y[i] - from_y[i]);
    }
}

template <Numeric T>
void lerp_3d(T const* RESTRICT from_x,
             T const* RESTRICT from_y,
             T const* RESTRICT from_z,
             T const* RESTRICT to_x,
             T const* RESTRICT to_y,
             T const* RESTRICT to_z,
             T const* RESTRICT alpha,
             T* RESTRICT out_x,
             T* RESTRICT out_y,
             T* RESTRICT out_z,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out_x[i] = from_x[i] + alpha[i] * (to_x[i] - from_x[i]);
        out_y[i] = from_y[i] + alpha[i] * (to_y[i] - from_y[i]);
        out_z[i] = from_z[i] + alpha[i] * (to_z[i] - from_z[i]);
    }
}

template <Numeric T>
void lerp_3d(T const* RESTRICT from_x,
             T const* RESTRICT from_y,
             T const* RESTRICT from_z,
             T const* RESTRICT to_x,
             T const* RESTRICT to_y,
             T const* RESTRICT to_z,
             T const alpha,
             T* RESTRICT out_x,
             T* RESTRICT out_y,
             T* RESTRICT out_z,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out_x[i] = from_x[i] + alpha * (to_x[i] - from_x[i]);
        out_y[i] = from_y[i] + alpha * (to_y[i] - from_y[i]);
        out_z[i] = from_z[i] + alpha * (to_z[i] - from_z[i]);
    }
}
}

namespace ml {
template <Numeric T>
bool lerp_1d(TConstArrayView<T> from,
             TConstArrayView<T> to,
             TConstArrayView<T> alpha,
             TArrayView<T> out) noexcept {
    auto const count{from.Num()};

    if (!ml::detail::all_num_equal(count, to, alpha, out)) {
        return false;
    }

    ml::kernel::lerp_1d(from.GetData(), to.GetData(), alpha.GetData(), out.GetData(), count);

    return true;
}

template <Numeric T>
bool lerp_1d(TConstArrayView<T> from,
             TConstArrayView<T> to,
             T const alpha,
             TArrayView<T> out) noexcept {
    auto const count{from.Num()};

    if (!ml::detail::all_num_equal(count, to, out)) {
        return false;
    }

    ml::kernel::lerp_1d(from.GetData(), to.GetData(), alpha, out.GetData(), count);

    return true;
}

template <Numeric T>
bool lerp_2d(TConstArrayView<T> from_x,
             TConstArrayView<T> from_y,
             TConstArrayView<T> to_x,
             TConstArrayView<T> to_y,
             TConstArrayView<T> alpha,
             TArrayView<T> out_x,
             TArrayView<T> out_y) noexcept {
    auto const count{from_x.Num()};

    if (!ml::detail::all_num_equal(count, from_y, to_x, to_y, alpha, out_x, out_y)) {
        return false;
    }

    ml::kernel::lerp_2d(from_x.GetData(),
                        from_y.GetData(),
                        to_x.GetData(),
                        to_y.GetData(),
                        alpha.GetData(),
                        out_x.GetData(),
                        out_y.GetData(),
                        count);

    return true;
}

template <Numeric T>
bool lerp_2d(TConstArrayView<T> from_x,
             TConstArrayView<T> from_y,
             TConstArrayView<T> to_x,
             TConstArrayView<T> to_y,
             T const alpha,
             TArrayView<T> out_x,
             TArrayView<T> out_y) noexcept {
    auto const count{from_x.Num()};

    if (!ml::detail::all_num_equal(count, from_y, to_x, to_y, out_x, out_y)) {
        return false;
    }

    ml::kernel::lerp_2d(from_x.GetData(),
                        from_y.GetData(),
                        to_x.GetData(),
                        to_y.GetData(),
                        alpha,
                        out_x.GetData(),
                        out_y.GetData(),
                        count);

    return true;
}

template <Numeric T>
bool lerp_3d(TConstArrayView<T> from_x,
             TConstArrayView<T> from_y,
             TConstArrayView<T> from_z,
             TConstArrayView<T> to_x,
             TConstArrayView<T> to_y,
             TConstArrayView<T> to_z,
             TConstArrayView<T> alpha,
             TArrayView<T> out_x,
             TArrayView<T> out_y,
             TArrayView<T> out_z) noexcept {
    auto const count{from_x.Num()};

    if (!ml::detail::all_num_equal(
            count, from_y, from_z, to_x, to_y, to_z, alpha, out_x, out_y, out_z)) {
        return false;
    }

    ml::kernel::lerp_3d(from_x.GetData(),
                        from_y.GetData(),
                        from_z.GetData(),
                        to_x.GetData(),
                        to_y.GetData(),
                        to_z.GetData(),
                        alpha.GetData(),
                        out_x.GetData(),
                        out_y.GetData(),
                        out_z.GetData(),
                        count);

    return true;
}

template <Numeric T>
bool lerp_3d(TConstArrayView<T> from_x,
             TConstArrayView<T> from_y,
             TConstArrayView<T> from_z,
             TConstArrayView<T> to_x,
             TConstArrayView<T> to_y,
             TConstArrayView<T> to_z,
             T const alpha,
             TArrayView<T> out_x,
             TArrayView<T> out_y,
             TArrayView<T> out_z) noexcept {
    auto const count{from_x.Num()};

    if (!ml::detail::all_num_equal(count, from_y, from_z, to_x, to_y, to_z, out_x, out_y, out_z)) {
        return false;
    }

    ml::kernel::lerp_3d(from_x.GetData(),
                        from_y.GetData(),
                        from_z.GetData(),
                        to_x.GetData(),
                        to_y.GetData(),
                        to_z.GetData(),
                        alpha,
                        out_x.GetData(),
                        out_y.GetData(),
                        out_z.GetData(),
                        count);

    return true;
}
}
