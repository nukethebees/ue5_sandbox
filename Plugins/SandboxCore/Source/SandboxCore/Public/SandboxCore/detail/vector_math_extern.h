#pragma once

#include "vector_math.tpp"

namespace ml::kernel {
#define ML_EXTERN_FN(T)                                                    \
    extern template SANDBOXCORE_API void add_vector3<T>(T const* RESTRICT, \
                                                        T const* RESTRICT, \
                                                        T const* RESTRICT, \
                                                        T const* RESTRICT, \
                                                        T const* RESTRICT, \
                                                        T const* RESTRICT, \
                                                        T* RESTRICT,       \
                                                        T* RESTRICT,       \
                                                        T* RESTRICT,       \
                                                        int32 const) noexcept
ML_EXTERN_FN(uint8);
ML_EXTERN_FN(float);
ML_EXTERN_FN(double);
ML_EXTERN_FN(int32);
#undef ML_EXTERN_FN

#define ML_EXTERN_FN(T)                                                                  \
    extern template SANDBOXCORE_API void add_vector3_in_place<T>(T * dst_x,               \
                                                                T * dst_y,               \
                                                                T * dst_z,               \
                                                                T const* RESTRICT src_x, \
                                                                T const* RESTRICT src_y, \
                                                                T const* RESTRICT src_z, \
                                                                int32 const count) noexcept
ML_EXTERN_FN(uint8);
ML_EXTERN_FN(float);
ML_EXTERN_FN(double);
ML_EXTERN_FN(int32);
#undef ML_EXTERN_FN

}