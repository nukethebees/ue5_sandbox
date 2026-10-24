#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/constrained_strings.h>
#include <ioj/constrained_vectors.h>

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace ioj::s7::constraints {
template <typename L, typename R>
auto less_equal(L const left, R const right) -> bool {
    if constexpr (std::integral<L> && std::integral<R>) {
        return std::cmp_less_equal(left, right);
    } else if constexpr (std::floating_point<L> && std::floating_point<R>) {
        return left <= right;
    } else {
        using Integer = std::conditional_t<std::integral<L>, L, R>;
        auto const upper{std::ldexp(1.0L, std::numeric_limits<Integer>::digits)};
        auto const lower{std::is_signed_v<Integer> ? -upper : 0.0L};
        // Round toward the comparison boundary before converting to avoid precision loss at
        // 2^63/2^64.
        if constexpr (std::integral<L>) {
            auto const bound{std::floor(static_cast<long double>(right))};
            if (std::isnan(bound) || bound < lower) {
                return false;
            }
            return bound >= upper || left <= static_cast<Integer>(bound);
        } else {
            auto const bound{std::ceil(static_cast<long double>(left))};
            if (std::isnan(bound) || bound >= upper) {
                return false;
            }
            return bound < lower || static_cast<Integer>(bound) <= right;
        }
    }
}

template <typename T, typename Predicate>
auto check(T const& value, Predicate predicate) -> bool {
    return predicate(value);
}
template <typename T, ConstrainedValuePolicy Policy, typename Predicate>
auto check(ConstrainedValue<T, Policy> const& value, Predicate predicate) -> bool {
    return predicate(value.get_ref());
}
template <typename T, typename Predicate>
auto check(std::optional<T> const& value, Predicate predicate) -> bool {
    return !value || check(*value, predicate);
}
template <typename T>
auto nonzero(T const& value) -> bool {
    return check(value, []<typename U>(U const& number) {
        static_assert(std::is_arithmetic_v<U> && !std::same_as<U, bool>,
                      "nonzero requires a number");
        return number != 0;
    });
}
template <typename T>
auto positive(T const& value) -> bool {
    return check(value, []<typename U>(U const& number) {
        return Positive<U>::try_make(number).has_value();
    });
}
template <typename T>
auto nonnegative(T const& value) -> bool {
    return check(value, []<typename U>(U const& number) {
        return NonNegative<U>::try_make(number).has_value();
    });
}
template <typename T>
auto finite(T const& value) -> bool {
    return check(value, []<typename U>(U const& number) {
        if constexpr (requires { number.elements; }) {
            return ConstrainedValue<U, ioj::detail::finite_vector_policy>::try_make(number)
                .has_value();
        } else if constexpr (std::floating_point<U>) {
            return std::isfinite(number);
        } else {
            static_assert(std::integral<U> && !std::same_as<U, bool>,
                          "finite requires a number or vector");
            return true;
        }
    });
}
template <typename T>
auto nonempty(T const& value) -> bool {
    return check(value, []<typename U>(U const& text) {
        static_assert(std::same_as<U, std::string>, "nonempty requires a string");
        return NonEmptyString::try_make(text).has_value();
    });
}

template <typename T, typename Bound>
auto minimum(T const& value, Bound const bound) -> bool {
    return check(value, [bound]<typename U>(U const& number) {
        static_assert(std::is_arithmetic_v<U> && !std::same_as<U, bool>,
                      "minimum requires a number");
        return less_equal(bound, number);
    });
}
template <typename T, typename Bound>
auto maximum(T const& value, Bound const bound) -> bool {
    return check(value, [bound]<typename U>(U const& number) {
        static_assert(std::is_arithmetic_v<U> && !std::same_as<U, bool>,
                      "maximum requires a number");
        return less_equal(number, bound);
    });
}
}
