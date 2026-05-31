#include "SandboxCore/rotation.h"

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

#define ML_EXTERN_FN(T)                                                    \
    template SANDBOXCORE_API void rotate_towards_1d_degrees_normalised<T>( \
        T const* RESTRICT current,                                         \
        T const* RESTRICT target,                                          \
        T const speed,                                                     \
        T const delta_time,                                                \
        T* RESTRICT out,                                                   \
        int32 const count) noexcept
ML_EXTERN_FN(float);
#undef ML_EXTERN_FN

#define ML_EXTERN_FN(T)                                                            \
    template SANDBOXCORE_API void rotate_towards_1d_degrees_normalised_in_place<T>( \
        T * RESTRICT current,                                                      \
        T const* RESTRICT target,                                                  \
        T const speed,                                                             \
        T const delta_time,                                                        \
        int32 const count) noexcept
ML_EXTERN_FN(float);
#undef ML_EXTERN_FN

#define ML_EXTERN_FN(T)                                            \
    template SANDBOXCORE_API void compute_desired_yaws_radians<T>( \
        T const* RESTRICT const start_xs,                          \
        T const* RESTRICT const start_ys,                          \
        T const* RESTRICT const end_xs,                            \
        T const* RESTRICT const end_ys,                            \
        T* RESTRICT const out_yaws_radians,                        \
        int32 const count) noexcept
ML_EXTERN_FN(float);
#undef ML_EXTERN_FN
}
