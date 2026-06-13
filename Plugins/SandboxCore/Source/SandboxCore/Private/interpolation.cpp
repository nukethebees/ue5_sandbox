#include "SandboxCore/interpolation.h"

namespace ml::kernel {
#define ML_INSTANTIATE_FN(T)                                         \
    template SANDBOXCORE_API void lerp_1d<T>(T* RESTRICT out,        \
                                             T const* RESTRICT from, \
                                             T const* RESTRICT to,   \
                                             T const* RESTRICT alpha, \
                                             int32 const count) noexcept
ML_INSTANTIATE_FN(float);
ML_INSTANTIATE_FN(double);
#undef ML_INSTANTIATE_FN
}
