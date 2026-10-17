#pragma once

#include <cmath>
#include <concepts>

namespace ioj {
template <typename T>
struct MinMax {
    T min{};
    T max{1};

    auto operator==(MinMax const&) const -> bool = default;
};

template <std::floating_point T>
[[nodiscard]] auto is_finite(MinMax<T> const range) noexcept -> bool {
    return std::isfinite(range.min) && std::isfinite(range.max);
}

// Require finite endpoints and a strictly positive interval width.
template <std::floating_point T>
[[nodiscard]] auto is_positive_finite(MinMax<T> const range) noexcept -> bool {
    return is_finite(range) && range.max > range.min;
}
}
