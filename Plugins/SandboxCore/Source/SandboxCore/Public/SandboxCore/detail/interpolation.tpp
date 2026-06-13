#pragma once

#include "SandboxCore/array_utils.h"
#include "SandboxCore/numeric.h"

#include "CoreMinimal.h"

namespace ml::kernel {
template <Numeric T>
void lerp_1d(T* RESTRICT out,
             T const* RESTRICT from,
             T const* RESTRICT to,
             T const* RESTRICT alpha,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = from[i] + alpha[i] * (to[i] - from[i]);
    }
}

#define ML_EXTERN_FN(T)                                                      \
    extern template SANDBOXCORE_API void lerp_1d<T>(T* RESTRICT out,         \
                                                    T const* RESTRICT from,  \
                                                    T const* RESTRICT to,    \
                                                    T const* RESTRICT alpha, \
                                                    int32 const count) noexcept
ML_EXTERN_FN(float);
ML_EXTERN_FN(double);
#undef ML_EXTERN_FN

template <Numeric T>
void lerp_1d(T* RESTRICT out,
             T const* RESTRICT from,
             T const* RESTRICT to,
             T alpha,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out[i] = from[i] + alpha * (to[i] - from[i]);
    }
}

template <Numeric T>
void lerp_1d_in_place(T* current,
                      T const* target,
                      T const* alpha,
                      int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        current[i] += alpha[i] * (target[i] - current[i]);
    }
}

template <Numeric T>
void lerp_1d_in_place(T* current, T const* target, T const alpha, int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        current[i] += alpha * (target[i] - current[i]);
    }
}

template <Numeric T>
void lerp_2d(T* RESTRICT out_x,
             T* RESTRICT out_y,
             T const* RESTRICT from_x,
             T const* RESTRICT from_y,
             T const* RESTRICT to_x,
             T const* RESTRICT to_y,
             T const* RESTRICT alpha,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out_x[i] = from_x[i] + alpha[i] * (to_x[i] - from_x[i]);
        out_y[i] = from_y[i] + alpha[i] * (to_y[i] - from_y[i]);
    }
}

template <Numeric T>
void lerp_2d(T* RESTRICT out_x,
             T* RESTRICT out_y,
             T const* RESTRICT from_x,
             T const* RESTRICT from_y,
             T const* RESTRICT to_x,
             T const* RESTRICT to_y,
             T const alpha,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out_x[i] = from_x[i] + alpha * (to_x[i] - from_x[i]);
        out_y[i] = from_y[i] + alpha * (to_y[i] - from_y[i]);
    }
}

template <Numeric T>
void lerp_2d_in_place(T* current_x,
                      T* current_y,
                      T const* target_x,
                      T const* target_y,
                      T const* alpha,
                      int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        current_x[i] += alpha[i] * (target_x[i] - current_x[i]);
        current_y[i] += alpha[i] * (target_y[i] - current_y[i]);
    }
}

template <Numeric T>
void lerp_2d_in_place(T* current_x,
                      T* current_y,
                      T const* target_x,
                      T const* target_y,
                      T const alpha,
                      int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        current_x[i] += alpha * (target_x[i] - current_x[i]);
        current_y[i] += alpha * (target_y[i] - current_y[i]);
    }
}

template <Numeric T>
void lerp_3d(T* RESTRICT out_x,
             T* RESTRICT out_y,
             T* RESTRICT out_z,
             T const* RESTRICT from_x,
             T const* RESTRICT from_y,
             T const* RESTRICT from_z,
             T const* RESTRICT to_x,
             T const* RESTRICT to_y,
             T const* RESTRICT to_z,
             T const* RESTRICT alpha,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out_x[i] = from_x[i] + alpha[i] * (to_x[i] - from_x[i]);
        out_y[i] = from_y[i] + alpha[i] * (to_y[i] - from_y[i]);
        out_z[i] = from_z[i] + alpha[i] * (to_z[i] - from_z[i]);
    }
}

template <Numeric T>
void lerp_3d(T* RESTRICT out_x,
             T* RESTRICT out_y,
             T* RESTRICT out_z,
             T const* RESTRICT from_x,
             T const* RESTRICT from_y,
             T const* RESTRICT from_z,
             T const* RESTRICT to_x,
             T const* RESTRICT to_y,
             T const* RESTRICT to_z,
             T const alpha,
             int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out_x[i] = from_x[i] + alpha * (to_x[i] - from_x[i]);
        out_y[i] = from_y[i] + alpha * (to_y[i] - from_y[i]);
        out_z[i] = from_z[i] + alpha * (to_z[i] - from_z[i]);
    }
}

template <Numeric T>
void lerp_3d_in_place(T* current_x,
                      T* current_y,
                      T* current_z,
                      T const* target_x,
                      T const* target_y,
                      T const* target_z,
                      T const* alpha,
                      int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        current_x[i] += alpha[i] * (target_x[i] - current_x[i]);
        current_y[i] += alpha[i] * (target_y[i] - current_y[i]);
        current_z[i] += alpha[i] * (target_z[i] - current_z[i]);
    }
}

template <Numeric T>
void lerp_3d_in_place(T* current_x,
                      T* current_y,
                      T* current_z,
                      T const* target_x,
                      T const* target_y,
                      T const* target_z,
                      T const alpha,
                      int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        current_x[i] += alpha * (target_x[i] - current_x[i]);
        current_y[i] += alpha * (target_y[i] - current_y[i]);
        current_z[i] += alpha * (target_z[i] - current_z[i]);
    }
}
}

namespace ml {
template <Numeric T>
void lerp_1d(TArrayView<T> out,
             TConstArrayView<T> from,
             TConstArrayView<T> to,
             TConstArrayView<T> alpha) noexcept {
    auto const count{out.Num()};

    check(ml::all_num_equal_to(count, from, to, alpha));

    ml::kernel::lerp_1d(out.GetData(), from.GetData(), to.GetData(), alpha.GetData(), count);
}

template <Numeric T>
void lerp_1d(TArrayView<T> out,
             TConstArrayView<T> from,
             TConstArrayView<T> to,
             T const alpha) noexcept {
    auto const count{out.Num()};

    check(ml::all_num_equal_to(count, from, to));

    ml::kernel::lerp_1d(out.GetData(), from.GetData(), to.GetData(), alpha, count);
}

template <Numeric T>
void lerp_1d_in_place(TArrayView<T> current,
                      TConstArrayView<T> target,
                      TConstArrayView<T> alpha) noexcept {
    auto const count{current.Num()};

    check(ml::all_num_equal_to(count, target, alpha));

    ml::kernel::lerp_1d_in_place(
        current.GetData(), target.GetData(), alpha.GetData(), count);
}

template <Numeric T>
void lerp_1d_in_place(TArrayView<T> current,
                      TConstArrayView<T> target,
                      T const alpha) noexcept {
    auto const count{current.Num()};

    check(ml::all_num_equal_to(count, target));

    ml::kernel::lerp_1d_in_place(current.GetData(), target.GetData(), alpha, count);
}

template <Numeric T>
void lerp_2d(TArrayView<T> out_x,
             TArrayView<T> out_y,
             TConstArrayView<T> from_x,
             TConstArrayView<T> from_y,
             TConstArrayView<T> to_x,
             TConstArrayView<T> to_y,
             TConstArrayView<T> alpha) noexcept {
    auto const count{out_x.Num()};

    check(ml::all_num_equal_to(count, out_y, from_x, from_y, to_x, to_y, alpha));

    ml::kernel::lerp_2d(out_x.GetData(),
                        out_y.GetData(),
                        from_x.GetData(),
                        from_y.GetData(),
                        to_x.GetData(),
                        to_y.GetData(),
                        alpha.GetData(),
                        count);
}

template <Numeric T>
void lerp_2d(TArrayView<T> out_x,
             TArrayView<T> out_y,
             TConstArrayView<T> from_x,
             TConstArrayView<T> from_y,
             TConstArrayView<T> to_x,
             TConstArrayView<T> to_y,
             T const alpha) noexcept {
    auto const count{out_x.Num()};

    check(ml::all_num_equal_to(count, out_y, from_x, from_y, to_x, to_y));

    ml::kernel::lerp_2d(out_x.GetData(),
                        out_y.GetData(),
                        from_x.GetData(),
                        from_y.GetData(),
                        to_x.GetData(),
                        to_y.GetData(),
                        alpha,
                        count);
}

template <Numeric T>
void lerp_2d_in_place(TArrayView<T> current_x,
                      TArrayView<T> current_y,
                      TConstArrayView<T> target_x,
                      TConstArrayView<T> target_y,
                      TConstArrayView<T> alpha) noexcept {
    auto const count{current_x.Num()};

    check(ml::all_num_equal_to(count, current_y, target_x, target_y, alpha));

    ml::kernel::lerp_2d_in_place(current_x.GetData(),
                                 current_y.GetData(),
                                 target_x.GetData(),
                                 target_y.GetData(),
                                 alpha.GetData(),
                                 count);
}

template <Numeric T>
void lerp_2d_in_place(TArrayView<T> current_x,
                      TArrayView<T> current_y,
                      TConstArrayView<T> target_x,
                      TConstArrayView<T> target_y,
                      T const alpha) noexcept {
    auto const count{current_x.Num()};

    check(ml::all_num_equal_to(count, current_y, target_x, target_y));

    ml::kernel::lerp_2d_in_place(current_x.GetData(),
                                 current_y.GetData(),
                                 target_x.GetData(),
                                 target_y.GetData(),
                                 alpha,
                                 count);
}

template <Numeric T>
void lerp_3d(TArrayView<T> out_x,
             TArrayView<T> out_y,
             TArrayView<T> out_z,
             TConstArrayView<T> from_x,
             TConstArrayView<T> from_y,
             TConstArrayView<T> from_z,
             TConstArrayView<T> to_x,
             TConstArrayView<T> to_y,
             TConstArrayView<T> to_z,
             TConstArrayView<T> alpha) noexcept {
    auto const count{out_x.Num()};

    check(ml::all_num_equal_to(
        count, out_y, out_z, from_x, from_y, from_z, to_x, to_y, to_z, alpha));

    ml::kernel::lerp_3d(out_x.GetData(),
                        out_y.GetData(),
                        out_z.GetData(),
                        from_x.GetData(),
                        from_y.GetData(),
                        from_z.GetData(),
                        to_x.GetData(),
                        to_y.GetData(),
                        to_z.GetData(),
                        alpha.GetData(),
                        count);
}

template <Numeric T>
void lerp_3d(TArrayView<T> out_x,
             TArrayView<T> out_y,
             TArrayView<T> out_z,
             TConstArrayView<T> from_x,
             TConstArrayView<T> from_y,
             TConstArrayView<T> from_z,
             TConstArrayView<T> to_x,
             TConstArrayView<T> to_y,
             TConstArrayView<T> to_z,
             T const alpha) noexcept {
    auto const count{out_x.Num()};

    check(
        ml::all_num_equal_to(count, out_y, out_z, from_x, from_y, from_z, to_x, to_y, to_z));

    ml::kernel::lerp_3d(out_x.GetData(),
                        out_y.GetData(),
                        out_z.GetData(),
                        from_x.GetData(),
                        from_y.GetData(),
                        from_z.GetData(),
                        to_x.GetData(),
                        to_y.GetData(),
                        to_z.GetData(),
                        alpha,
                        count);
}

template <Numeric T>
void lerp_3d_in_place(TArrayView<T> current_x,
                      TArrayView<T> current_y,
                      TArrayView<T> current_z,
                      TConstArrayView<T> target_x,
                      TConstArrayView<T> target_y,
                      TConstArrayView<T> target_z,
                      TConstArrayView<T> alpha) noexcept {
    auto const count{current_x.Num()};

    check(ml::all_num_equal_to(count, current_y, current_z, target_x, target_y, target_z, alpha));

    ml::kernel::lerp_3d_in_place(current_x.GetData(),
                                 current_y.GetData(),
                                 current_z.GetData(),
                                 target_x.GetData(),
                                 target_y.GetData(),
                                 target_z.GetData(),
                                 alpha.GetData(),
                                 count);
}

template <Numeric T>
void lerp_3d_in_place(TArrayView<T> current_x,
                      TArrayView<T> current_y,
                      TArrayView<T> current_z,
                      TConstArrayView<T> target_x,
                      TConstArrayView<T> target_y,
                      TConstArrayView<T> target_z,
                      T const alpha) noexcept {
    auto const count{current_x.Num()};

    check(ml::all_num_equal_to(count, current_y, current_z, target_x, target_y, target_z));

    ml::kernel::lerp_3d_in_place(current_x.GetData(),
                                 current_y.GetData(),
                                 current_z.GetData(),
                                 target_x.GetData(),
                                 target_y.GetData(),
                                 target_z.GetData(),
                                 alpha,
                                 count);
}
}
