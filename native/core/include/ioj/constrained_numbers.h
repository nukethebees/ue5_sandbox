#pragma once

#include <ioj/constrained_value.h>
#include <ioj/numeric.h>

#include <limits>

namespace ioj {
namespace detail {
inline constexpr auto finite_number_policy{
    ConstrainedValuePolicy{[](auto const value) constexpr noexcept -> bool {
                               return value >= std::numeric_limits<decltype(value)>::lowest() &&
                                      value <= std::numeric_limits<decltype(value)>::max();
                           },
                           "Number must be finite"}};

template <auto Minimum, auto Maximum>
struct BoundedNumber {
    [[nodiscard]] constexpr auto operator()(auto const value) const noexcept -> bool {
        return value >= Minimum && value <= Maximum;
    }
};

template <auto Minimum, auto Maximum>
struct OpenIntervalNumber {
    [[nodiscard]] constexpr auto operator()(auto const value) const noexcept -> bool {
        return value > Minimum && value < Maximum;
    }
};

template <auto Minimum, auto Maximum>
inline constexpr auto bounded_number_policy{ConstrainedValuePolicy{
    BoundedNumber<Minimum, Maximum>{}, "Number must be within the permitted closed interval"}};

template <auto Minimum, auto Maximum>
inline constexpr auto open_interval_number_policy{ConstrainedValuePolicy{
    OpenIntervalNumber<Minimum, Maximum>{}, "Number must be within the permitted open interval"}};

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
using Finite = ConstrainedValue<T, detail::finite_number_policy>;

template <Numeric T, auto Minimum, auto Maximum>
    requires (!std::same_as<T, bool> && Minimum <= Maximum)
using Bounded = ConstrainedValue<T, detail::bounded_number_policy<Minimum, Maximum>>;

template <Numeric T, auto Minimum, auto Maximum>
    requires (!std::same_as<T, bool> && Minimum < Maximum)
using OpenInterval = ConstrainedValue<T, detail::open_interval_number_policy<Minimum, Maximum>>;

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
