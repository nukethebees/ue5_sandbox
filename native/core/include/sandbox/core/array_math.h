#pragma once

#include <sandbox/core/generated/array_math_kernels.h>

#include <cstdint>

namespace ml::kernel {
template <typename T>
auto collect_indices_less_equal(T const* values,
                                std::int32_t const count,
                                T const threshold,
                                std::int32_t* out_indices) noexcept -> std::int32_t {
    auto* const original{out_indices};
    for (std::int32_t i{}; i < count; ++i) {
        if (values[i] <= threshold) {
            *out_indices++ = i;
        }
    }
    return static_cast<std::int32_t>(out_indices - original);
}

template <typename T>
auto collect_values_not_equal(T const* values,
                              std::int32_t const count,
                              T const reference_value,
                              T* out_values) noexcept -> std::int32_t {
    auto* const original{out_values};
    for (std::int32_t i{}; i < count; ++i) {
        if (values[i] != reference_value) {
            *out_values++ = values[i];
        }
    }
    return static_cast<std::int32_t>(out_values - original);
}

template <typename T>
auto sum(T const* values, std::int32_t const count) noexcept -> T {
    T out{};
    for (std::int32_t i{}; i < count; ++i) {
        out += values[i];
    }
    return out;
}
}
