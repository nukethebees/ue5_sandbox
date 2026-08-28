#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"

#include <utility>
#include <vector>

namespace ml {

template <typename T>
void apply_permutation(TArray<T>& values, TArrayView<int32> const indices) {
    std::vector<T> reordered;
    reordered.reserve(static_cast<std::size_t>(indices.Num()));
    for (int32 index{}; index < indices.Num(); ++index) {
        reordered.push_back(values[indices[index]]);
    }
    for (int32 index{}; index < indices.Num(); ++index) {
        values[index] = std::move(reordered[static_cast<std::size_t>(index)]);
    }
}

template <typename T>
    requires requires(T& values, TArrayView<int32> const indices) {
        values.apply_permutation(indices);
    }
void apply_permutation(T& values, TArrayView<int32> const indices) {
    values.apply_permutation(indices);
}

} // namespace ml
