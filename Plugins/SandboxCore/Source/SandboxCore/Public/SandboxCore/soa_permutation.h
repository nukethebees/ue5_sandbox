#pragma once

#include <sandbox/core/soa_permutation.h>

#include <concepts>
#include <Containers/Array.h>
#include <Containers/ArrayView.h>
#include <span>
#include <utility>

namespace ml {
namespace detail {
template <typename T>
concept SupportsApplyPermutation = requires(T& value, TArrayView<int32> indices) {
    { value.apply_permutation(indices) } -> std::same_as<void>;
};
}

template <typename T, typename Allocator>
void apply_permutation(TArray<T, Allocator>& values, TArrayView<int32> indices) {
    ml::apply_permutation(
        std::span<T>{values.GetData(), static_cast<std::size_t>(values.Num())},
        std::span<int32>{indices.GetData(), static_cast<std::size_t>(indices.Num())});
}

template <detail::SupportsApplyPermutation T>
void apply_permutation(T& value, TArrayView<int32> indices) {
    value.apply_permutation(indices);
}
}
