#pragma once

#include <ioj/numeric.h>
#include <ioj/require.h>

#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <limits>
#include <optional>
#include <source_location>
#include <type_traits>

namespace ioj {
namespace detail {

template <typename Checker, std::size_t MessageSize>
struct ConstrainedNumberPolicy {
    Checker check;
    char message[MessageSize];
};

// Sign-only floating-point constraints permit positive infinity and reject NaN.
inline constexpr auto positive_number_policy{ConstrainedNumberPolicy{
    [](auto const value) constexpr noexcept -> bool { return value > decltype(value){}; },
    "Number must be positive"}};

inline constexpr auto nonnegative_number_policy{ConstrainedNumberPolicy{
    [](auto const value) constexpr noexcept -> bool { return value >= decltype(value){}; },
    "Number must be nonnegative"}};

inline constexpr auto positive_finite_number_policy{ConstrainedNumberPolicy{
    [](auto const value) constexpr noexcept -> bool {
        return value > decltype(value){} && value <= std::numeric_limits<decltype(value)>::max();
    },
    "Number must be finite and positive"}};

inline constexpr auto nonnegative_finite_number_policy{ConstrainedNumberPolicy{
    [](auto const value) constexpr noexcept -> bool {
        return value >= decltype(value){} && value <= std::numeric_limits<decltype(value)>::max();
    },
    "Number must be finite and nonnegative"}};

template <Numeric T, ConstrainedNumberPolicy policy>
    requires (!std::same_as<T, bool> && std::same_as<T, std::remove_cv_t<T>>)
class ConstrainedNumber {
  public:
    using value_type = T;

    constexpr ConstrainedNumber() noexcept
        requires (policy.check(value_type{}))
    = default;

    // Reject invalid literal values during compilation.
    explicit consteval ConstrainedNumber(value_type const value) noexcept
        : value_{value} {
        if (!valid(value)) {
            std::abort();
        }
    }

    [[nodiscard]] static constexpr auto valid(value_type const value) noexcept -> bool {
        return policy.check(value);
    }

    // Require the value to satisfy the policy.
    [[nodiscard]] static auto
        make(value_type const value,
             std::source_location const location = std::source_location::current()) noexcept
        -> ConstrainedNumber {
        require(valid(value), policy.message, location);
        return ConstrainedNumber{value, Validated{}};
    }

    // Return nullopt if the value violates the policy.
    [[nodiscard]] static constexpr auto try_make(value_type const value) noexcept
        -> std::optional<ConstrainedNumber> {
        if (!valid(value)) {
            return std::nullopt;
        }
        return ConstrainedNumber{value, Validated{}};
    }

    [[nodiscard]] constexpr auto get() const noexcept -> value_type { return value_; }
    [[nodiscard]] constexpr operator value_type() const noexcept { return value_; }
  private:
    struct Validated {};

    explicit constexpr ConstrainedNumber(value_type const value, Validated) noexcept
        : value_{value} {}

    value_type value_{};
};
}

template <Numeric T>
using Positive = detail::ConstrainedNumber<T, detail::positive_number_policy>;

template <Numeric T>
using NonNegative = detail::ConstrainedNumber<T, detail::nonnegative_number_policy>;

template <Numeric T>
using PositiveFinite = detail::ConstrainedNumber<T, detail::positive_finite_number_policy>;

template <Numeric T>
using NonNegativeFinite = detail::ConstrainedNumber<T, detail::nonnegative_finite_number_policy>;
}

template <ioj::Numeric T, ioj::detail::ConstrainedNumberPolicy policy, typename Char>
struct std::formatter<ioj::detail::ConstrainedNumber<T, policy>, Char> : std::formatter<T, Char> {
    template <typename FormatContext>
    auto format(ioj::detail::ConstrainedNumber<T, policy> const value, FormatContext& context) const
        -> typename FormatContext::iterator {
        return std::formatter<T, Char>::format(value.get(), context);
    }
};
