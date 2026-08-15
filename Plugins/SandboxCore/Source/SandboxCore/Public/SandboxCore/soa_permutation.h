#pragma once

#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <CoreMinimal.h>

#include <concepts>
#include <utility>

namespace ml {
namespace detail {
template <typename T>
concept SupportsApplyPermutation = requires(T& value, TArrayView<int32> indices) {
    { value.apply_permutation(indices) } -> std::same_as<void>;
};
}

// indices[new_index] is the old row index that belongs at new_index.
// The indices are restored before returning so one permutation can be applied to every stream.
template <typename T, typename Allocator>
void apply_permutation(TArray<T, Allocator>& values, TArrayView<int32> indices) {
    auto const n{values.Num()};
    check(indices.Num() == n);

    for (int32 start_index{}; start_index < n; ++start_index) {
        if (indices[start_index] < 0) {
            continue;
        }

        auto value{MoveTemp(values[start_index])};
        auto destination_index{start_index};
        while (true) {
            auto const source_index{indices[destination_index]};
            check((source_index >= 0) && (source_index < n));
            indices[destination_index] = ~source_index;

            if (source_index == start_index) {
                values[destination_index] = MoveTemp(value);
                break;
            }

            values[destination_index] = MoveTemp(values[source_index]);
            destination_index = source_index;
        }
    }

    for (auto& index : indices) {
        check(index < 0);
        index = ~index;
    }
}

template <detail::SupportsApplyPermutation T>
void apply_permutation(T& value, TArrayView<int32> indices) {
    value.apply_permutation(indices);
}
}
