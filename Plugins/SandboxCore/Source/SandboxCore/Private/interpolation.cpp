#include "SandboxCore/Public/interpolation.h"

namespace ml::kernel {
#define ML_INSTANTIATE_FN(T)                                       \
    template SANDBOXCORE_API void lerp_1d<T>(T const* RESTRICT from, \
                                             T const* RESTRICT to, \
                                             T const* RESTRICT alpha,  \
                                             T* RESTRICT out,      \
                                             int32 const count) noexcept
ML_INSTANTIATE_FN(float);
ML_INSTANTIATE_FN(double);
#undef ML_INSTANTIATE_FN
}
