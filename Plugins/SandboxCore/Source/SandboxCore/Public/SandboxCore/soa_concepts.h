#pragma once

#include <type_traits>

namespace ml {
struct FApplyArraysProbe {
    template <typename... T>
    void operator()(T&&...) const {}
};

struct FApplyArrayPairsProbe {
    template <typename... T>
    void operator()(T&&...) const {}
};

template <typename T>
concept SupportsApplyArrays =
    requires(std::remove_cvref_t<T>& value, std::remove_cvref_t<T> const& const_value) {
        value.apply_arrays(FApplyArraysProbe{});
        const_value.apply_arrays(FApplyArraysProbe{});
    };

template <typename T>
concept SupportsApplyArrayPairs =
    requires(std::remove_cvref_t<T>& value, std::remove_cvref_t<T> const& const_value) {
        value.apply_array_pairs(const_value, FApplyArrayPairsProbe{});
    };

template <typename T, typename Other>
concept SupportsApplyArrayPairsWith =
    requires(std::remove_cvref_t<T>& value, std::remove_cvref_t<Other> const& other) {
        value.apply_array_pairs(other, FApplyArrayPairsProbe{});
    };
} // namespace ml
