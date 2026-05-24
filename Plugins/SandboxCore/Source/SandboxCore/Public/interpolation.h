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
             int32 const count) {
    for (int32 i{0}; i < count; ++i) {
        out[i] = from[i] + alpha[i] * (to[i] - from[i]);
    }
}

#define ML_EXTERN_FN(T)                                                      \
    extern template SANDBOXCORE_API void lerp_1d<T>(T const* RESTRICT from,  \
                                                    T const* RESTRICT to,    \
                                                    T const* RESTRICT alpha, \
                                                    T* RESTRICT out,         \
                                                    int32 const count)
ML_EXTERN_FN(float);
ML_EXTERN_FN(double);
#undef ML_EXTERN_FN
}

namespace ml {

template <Numeric T>
bool lerp_1d(TConstArrayView<T> from,
             TConstArrayView<T> to,
             TConstArrayView<T> alpha,
             TArrayView<T> out) {
    auto const count{from.Num()};

    if (!ml::detail::all_num_equal(count, to, alpha, out)) {
        return false;
    }

    ml::kernel::lerp_1d(from.GetData(), to.GetData(), alpha.GetData(), out.GetData(), count);

    return true;
}
}
