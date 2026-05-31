#include "SandboxCore/Public/array_math.h"

namespace ml::detail {
#define ML_EXTERN_FN(T)                                \
    template SANDBOXCORE_API void subtract_in_place<T>( \
        T* data, T const value, int32 const count) noexcept

ML_EXTERN_FN(float);
#undef ML_EXTERN_FN
}
