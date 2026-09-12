#pragma once

#include "array_math.tpp"

namespace ml::kernel {
#define ML_EXTERN_FN(T)                                                                       \
    extern template auto collect_indices_less_equal<T>(                                       \
        T const* values, std::int32_t count, T threshold, std::int32_t* out_indices) noexcept \
        -> std::int32_t

ML_EXTERN_FN(float);
#undef ML_EXTERN_FN
}
