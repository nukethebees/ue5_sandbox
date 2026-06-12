#pragma once

#include <SandboxCore/numeric.h>

#include "CoreMinimal.h"

namespace ml::kernel {
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
void multiply_in_place(T* data, T const value, int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        data[i] *= value;
    }
}

template <ml::Numeric T>
void divide_in_place(T* data, T const value, int32 const count) noexcept {
    for (int32 i{0}; i < count; ++i) {
        data[i] /= value;
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

template <typename T>
auto collect_values_not_equal(T const* RESTRICT values,
                              int32 const count,
                              T const reference_value,
                              T* RESTRICT out_values) noexcept -> int32 {
    int32 const* const RESTRICT original{out_values};

    for (int32 i{0}; i < count; ++i) {
        if (values[i] != reference_value) {
            *out_values = values[i];
            out_values++;
        }
    }

    return static_cast<int32>(out_values - original);
}
}

namespace ml {
template <ml::Numeric T>
void add_in_place(TArrayView<T> data, T const value) noexcept {
    ml::kernel::add_in_place(data.GetData(), value, data.Num());
}

template <ml::Numeric T>
void subtract_in_place(TArrayView<T> data, T const value) noexcept {
    ml::kernel::subtract_in_place(data.GetData(), value, data.Num());
}

template <ml::Numeric T>
void multiply_in_place(TArrayView<T> data, T const value) noexcept {
    ml::kernel::multiply_in_place(data.GetData(), value, data.Num());
}

template <ml::Numeric T>
void divide_in_place(TArrayView<T> data, T const value) noexcept {
    ml::kernel::divide_in_place(data.GetData(), value, data.Num());
}

template <ml::Numeric T>
auto collect_indices_less_equal(TConstArrayView<T> values,
                                T const threshold,
                                TArrayView<int32> out_indices) noexcept -> TConstArrayView<int32> {
    auto const count{ml::kernel::collect_indices_less_equal(
        values.GetData(), values.Num(), threshold, out_indices.GetData())};

    return TConstArrayView<int32>{out_indices.Left(count)};
}

template <typename T>
auto collect_values_not_equal(TConstArrayView<T> values,
                              T const reference_value,
                              TArrayView<T> out_values) noexcept -> TConstArrayView<int32> {
    checkf(out_values.Num() >= values.Num(), TEXT("Insufficient size for out_values"));

    auto const count{ml::kernel::collect_values_not_equal(
        values.GetData(), values.Num(), reference_value, out_values.GetData())};

    return TConstArrayView<int32>{out_values.Left(count)};
}
}
