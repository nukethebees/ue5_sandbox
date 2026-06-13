#pragma once

#include "interpolation.tpp"

namespace ml::kernel {
#define ML_EXTERN_FN(T)                                                      \
    extern template SANDBOXCORE_API void lerp_1d<T>(T* RESTRICT out,         \
                                                    T const* RESTRICT from,  \
                                                    T const* RESTRICT to,    \
                                                    T const* RESTRICT alpha, \
                                                    int32 const count) noexcept
ML_EXTERN_FN(float);
ML_EXTERN_FN(double);
#undef ML_EXTERN_FN
}
