#pragma once

#include <sandbox/core/array_math.h>

#include "SandboxCore/array_checks.h"
#include <SandboxCore/numeric.h>

#include "CoreMinimal.h"

namespace ml {
// Replaces the output with ascending matching indices, retaining its capacity. Input and output
// must not alias. Unlike the view overload, storage grows only as matches are found.
template <ml::Numeric T>
auto collect_indices_less_equal(TConstArrayView<T> const values,
                                T const threshold,
                                TArray<int32>& out_indices) -> TConstArrayView<int32> {
    out_indices.Reset();
    auto const count{values.Num()};
    for (int32 index{}; index < count; ++index) {
        if (values[index] <= threshold) {
            out_indices.Add(index);
        }
    }
    return out_indices;
}

template <ml::Numeric T>
auto collect_indices_less_equal(TConstArrayView<T> const values,
                                T const threshold,
                                TArrayView<int32> const out_indices) noexcept
    -> TConstArrayView<int32> {
    auto const count{ml::kernel::collect_indices_less_equal(
        values.GetData(), values.Num(), threshold, out_indices.GetData())};

    return TConstArrayView<int32>{out_indices.Left(count)};
}

template <typename T>
auto collect_values_not_equal(TConstArrayView<T> const values,
                              T const reference_value,
                              TArrayView<T> const out_values) noexcept -> TConstArrayView<int32> {
    checkf(out_values.Num() >= values.Num(), TEXT("Insufficient size for out_values"));

    auto const count{ml::kernel::collect_values_not_equal(
        values.GetData(), values.Num(), reference_value, out_values.GetData())};

    return TConstArrayView<int32>{out_values.Left(count)};
}

template <typename T>
auto sum(TConstArrayView<T> const values) -> T {
    return ml::kernel::sum(values.GetData(), values.Num());
}
}
