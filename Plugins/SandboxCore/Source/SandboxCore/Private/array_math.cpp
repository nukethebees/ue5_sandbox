#include "SandboxCore/Public/array_math.h"

namespace ml::detail {
#define ML_EXTERN_FN(T)                                 \
    template SANDBOXCORE_API void subtract_in_place<T>( \
        T * data, T const value, int32 const count) noexcept

ML_EXTERN_FN(float);
#undef ML_EXTERN_FN

#define ML_EXTERN_FN(T)                                          \
    template SANDBOXCORE_API auto collect_indices_less_equal<T>( \
        T const* RESTRICT values,                                \
        int32 const count,                                       \
        T const threshold,                                       \
        int32* RESTRICT out_indices) noexcept -> int32

ML_EXTERN_FN(float);
#undef ML_EXTERN_FN
}
