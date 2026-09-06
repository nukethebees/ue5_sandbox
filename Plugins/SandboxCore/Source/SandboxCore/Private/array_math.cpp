#include "SandboxCore/array_math.h"

namespace ml::kernel {
#define ML_EXTERN_FN(T)                                          \
    template SANDBOXCORE_API auto collect_indices_less_equal<T>( \
        T const* RESTRICT values,                                \
        int32 const count,                                       \
        T const threshold,                                       \
        int32* RESTRICT out_indices) noexcept -> int32

ML_EXTERN_FN(float);
#undef ML_EXTERN_FN
}
