#pragma once

#include <SandboxCore/numeric.h>

#include "CoreMinimal.h"

namespace ml::detail {
template <ml::Numeric T>
void add_in_place(T* data, T const value, int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        data[i] += value;
    }
}

template <ml::Numeric T>
void subtract_in_place(T* data, T const value, int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        data[i] -= value;
    }
}

template <ml::Numeric T>
auto collect_indices_less_equal(T const* RESTRICT values,
                                int32 const count,
                                T const threshold,
                                int32* RESTRICT out_indices) noexcept -> int32 {
    int32 const* const RESTRICT original{out_indices};

    for (int32 i{0}; i < count; ++i) {
        if (values[i] <= threshold) {
            *out_indices = i;
            out_indices++;
        }
    }

    return static_cast<int32>(out_indices - original);
}
}

namespace ml {
template <ml::Numeric T>
void add_in_place(TArrayView<T> data, T const value) noexcept {
    ml::detail::add_in_place(data.GetData(), value, data.Num());
}

template <ml::Numeric T>
void subtract_in_place(TArrayView<T> data, T const value) noexcept {
    ml::detail::subtract_in_place(data.GetData(), value, data.Num());
}

template <ml::Numeric T>
auto collect_indices_less_equal(TConstArrayView<T> values,
                                T const threshold,
                                TArrayView<int32> out_indices) noexcept -> int32 {
    return ml::detail::collect_indices_less_equal(
        values.GetData(), values.Num(), threshold, out_indices.GetData());
}
}
