#pragma once

#include <ioj/constrained_value.h>
#include <ioj/numeric.h>

#include <limits>

namespace ioj {
namespace detail {
// Sign-only floating-point constraints permit positive infinity and reject NaN.
inline constexpr auto positive_number_policy{ConstrainedValuePolicy{
    [](auto const value) constexpr noexcept -> bool { return value > decltype(value){}; },
    "Number must be positive"}};

inline constexpr auto nonnegative_number_policy{ConstrainedValuePolicy{
    [](auto const value) constexpr noexcept -> bool { return value >= decltype(value){}; },
    "Number must be nonnegative"}};

inline constexpr auto positive_finite_number_policy{ConstrainedValuePolicy{
    [](auto const value) constexpr noexcept -> bool {
        return value > decltype(value){} && value <= std::numeric_limits<decltype(value)>::max();
    },
    "Number must be finite and positive"}};

inline constexpr auto nonnegative_finite_number_policy{ConstrainedValuePolicy{
    [](auto const value) constexpr noexcept -> bool {
        return value >= decltype(value){} && value <= std::numeric_limits<decltype(value)>::max();
    },
    "Number must be finite and nonnegative"}};

}

template <Numeric T>
    requires (!std::same_as<T, bool>)
using Positive = ConstrainedValue<T, detail::positive_number_policy>;

template <Numeric T>
    requires (!std::same_as<T, bool>)
using NonNegative = ConstrainedValue<T, detail::nonnegative_number_policy>;

template <Numeric T>
    requires (!std::same_as<T, bool>)
using PositiveFinite = ConstrainedValue<T, detail::positive_finite_number_policy>;

template <Numeric T>
    requires (!std::same_as<T, bool>)
using NonNegativeFinite = ConstrainedValue<T, detail::nonnegative_finite_number_policy>;
}
