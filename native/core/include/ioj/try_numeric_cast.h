#pragma once

#include <ioj/numeric.h>

#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace ioj {
// Preserve nonfinite floating values; callers impose domain-specific finite constraints.
template <Numeric To, Numeric From>
    requires (!std::same_as<To, bool> && !std::same_as<From, bool>)
[[nodiscard]] auto try_numeric_cast(From const value) -> std::optional<To> {
    if constexpr (std::integral<To> && std::integral<From>) {
        if (!std::in_range<To>(value)) {
            return std::nullopt;
        }
    } else if constexpr (std::integral<To>) {
        // Use an exclusive power-of-two bound: max() can round upwards in From.
        auto const upper{std::ldexp(From{1}, std::numeric_limits<To>::digits)};
        auto const lower{std::is_signed_v<To> ? -upper : From{0}};
        if (!std::isfinite(value) || std::trunc(value) != value || value < lower ||
            value >= upper) {
            return std::nullopt;
        }
    } else {
        auto const widened{static_cast<long double>(value)};
        if (std::isfinite(value) && (widened < std::numeric_limits<To>::lowest() ||
                                     widened > std::numeric_limits<To>::max())) {
            return std::nullopt;
        }
    }

    return static_cast<To>(value);
}
}
