#pragma once

#include <sandbox/core/generated/array_math_kernels.h>

#include <cstdint>
#include <type_traits>

namespace ml::kernel {
template <typename T, typename Index>
auto collect_indices_less_equal(T const* values,
                                std::type_identity_t<Index> const count,
                                T const threshold,
                                Index* out_indices) noexcept -> Index {
    auto* const original{out_indices};
    for (Index i{}; i < count; ++i) {
        if (values[i] <= threshold) {
            *out_indices++ = i;
        }
    }
    return static_cast<Index>(out_indices - original);
}

template <typename T>
auto collect_values_not_equal(T const* values,
                              std::uint32_t const count,
                              T const reference_value,
                              T* out_values) noexcept -> std::uint32_t {
    auto* const original{out_values};
    for (std::uint32_t i{}; i < count; ++i) {
        if (values[i] != reference_value) {
            *out_values++ = values[i];
        }
    }
    return static_cast<std::uint32_t>(out_values - original);
}

template <typename T>
auto sum(T const* values, std::uint32_t const count) noexcept -> T {
    T out{};
    for (std::uint32_t i{}; i < count; ++i) {
        out += values[i];
    }
    return out;
}
}
