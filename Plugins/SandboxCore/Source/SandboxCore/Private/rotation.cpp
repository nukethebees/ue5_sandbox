#include "rotation.h"

namespace ml::kernel {
#define ML_EXTERN_FN(T)                                                                   \
    template SANDBOXCORE_API void rotate_towards_1d_degrees<T>(T const* RESTRICT current, \
                                                               T const* RESTRICT target,  \
                                                               T const speed,             \
                                                               T const delta_time,        \
                                                               T* RESTRICT out,           \
                                                               int32 const count) noexcept
ML_EXTERN_FN(float);
#undef ML_EXTERN_FN
}
