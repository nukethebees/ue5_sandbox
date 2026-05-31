#pragma once

#include "array_utils.h"
#include "numeric.h"

#include "CoreMinimal.h"

namespace ml::kernel {
template <Numeric T>
void add_vector3(T const* RESTRICT lhs_x,
                 T const* RESTRICT lhs_y,
                 T const* RESTRICT lhs_z,
                 T const* RESTRICT rhs_x,
                 T const* RESTRICT rhs_y,
                 T const* RESTRICT rhs_z,
                 T* RESTRICT out_x,
                 T* RESTRICT out_y,
                 T* RESTRICT out_z,
                 int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        out_x[i] = lhs_x[i] + rhs_x[i];
        out_y[i] = lhs_y[i] + rhs_y[i];
        out_z[i] = lhs_z[i] + rhs_z[i];
    }
}

template <Numeric T>
void add_vector3_in_place(T* dst_x,
                         T* dst_y,
                         T* dst_z,
                         T const* RESTRICT src_x,
                         T const* RESTRICT src_y,
                         T const* RESTRICT src_z,
                         int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        dst_x[i] += src_x[i];
        dst_y[i] += src_y[i];
        dst_z[i] += src_z[i];
    }
}
}

namespace ml {

template <Numeric T>
bool add_vector3(TConstArrayView<T> lhs_x,
                 TConstArrayView<T> lhs_y,
                 TConstArrayView<T> lhs_z,
                 TConstArrayView<T> rhs_x,
                 TConstArrayView<T> rhs_y,
                 TConstArrayView<T> rhs_z,
                 TArrayView<T> out_x,
                 TArrayView<T> out_y,
                 TArrayView<T> out_z) noexcept {
    auto const count{lhs_x.Num()};

    if (!ml::all_num_equal_to(count, lhs_y, lhs_z, rhs_x, rhs_y, rhs_z, out_x, out_y, out_z)) {
        return false;
    }

    ml::kernel::add_vector3(lhs_x.GetData(),
                            lhs_y.GetData(),
                            lhs_z.GetData(),
                            rhs_x.GetData(),
                            rhs_y.GetData(),
                            rhs_z.GetData(),
                            out_x.GetData(),
                            out_y.GetData(),
                            out_z.GetData(),
                            count);

    return true;
}
}
