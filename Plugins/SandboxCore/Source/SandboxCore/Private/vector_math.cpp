#include "SandboxCore/vector_math.h"

namespace ml::kernel {
#define ML_INSTANTIATE_VEC3(T)                                      \
    template SANDBOXCORE_API void add_vector3<T>(T const* RESTRICT, \
                                                 T const* RESTRICT, \
                                                 T const* RESTRICT, \
                                                 T const* RESTRICT, \
                                                 T const* RESTRICT, \
                                                 T const* RESTRICT, \
                                                 T* RESTRICT,       \
                                                 T* RESTRICT,       \
                                                 T* RESTRICT,       \
                                                 int32 const) noexcept

ML_INSTANTIATE_VEC3(uint8);
ML_INSTANTIATE_VEC3(float);
ML_INSTANTIATE_VEC3(double);
ML_INSTANTIATE_VEC3(int32);
#undef ML_INSTANTIATE_VEC3

#define ML_INSTANTIATE_VEC3(T)                                                     \
    template SANDBOXCORE_API void add_vector3_in_place<T>(T * dst_x,               \
                                                          T * dst_y,               \
                                                          T * dst_z,               \
                                                          T const* RESTRICT src_x, \
                                                          T const* RESTRICT src_y, \
                                                          T const* RESTRICT src_z, \
                                                          int32 const count) noexcept
ML_INSTANTIATE_VEC3(uint8);
ML_INSTANTIATE_VEC3(float);
ML_INSTANTIATE_VEC3(double);
ML_INSTANTIATE_VEC3(int32);
#undef ML_INSTANTIATE_VEC3

#define ML_EXTERN_FN(T)                                   \
    template SANDBOXCORE_API void size_squared_vector<T>( \
        T const* RESTRICT vecs, int32 const count, VectorElementT<T>* RESTRICT out) noexcept
ML_EXTERN_FN(FVector);
#undef ML_EXTERN_FN

#define ML_EXTERN_FN(T)                                                        \
    template SANDBOXCORE_API void size_squared_vector<T>(T const* RESTRICT xs, \
                                                         T const* RESTRICT ys, \
                                                         T const* RESTRICT zs, \
                                                         int32 const count,    \
                                                         T* RESTRICT out) noexcept
ML_EXTERN_FN(float);
#undef ML_EXTERN_FN
}
