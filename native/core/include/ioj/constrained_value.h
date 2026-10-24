#pragma once

#include <ioj/require.h>

#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <optional>
#include <source_location>
#include <type_traits>

namespace ioj {
template <typename Checker, std::size_t MessageSize>
struct ConstrainedValuePolicy {
    Checker predicate;
    char message[MessageSize];
};

template <typename T, ConstrainedValuePolicy policy>
    requires (std::same_as<T, std::remove_cv_t<T>> && std::copyable<T>)
class ConstrainedValue {
  public:
    using value_type = T;

    constexpr ConstrainedValue() noexcept
        requires (policy.predicate(value_type{}))
    = default;

    // Reject invalid literal values during compilation.
    explicit consteval ConstrainedValue(value_type const value) noexcept
        : value_{value} {
        if (!valid(value)) {
            std::abort();
        }
    }

    [[nodiscard]] static constexpr auto valid(value_type const value) noexcept -> bool {
        return policy.predicate(value);
    }

    // Require the value to satisfy the policy.
    [[nodiscard]] static auto
        make(value_type const value,
             std::source_location const location = std::source_location::current()) noexcept
        -> ConstrainedValue {
        require(valid(value), policy.message, location);
        return ConstrainedValue{value, Validated{}};
    }

    // Return nullopt if the value violates the policy.
    [[nodiscard]] static constexpr auto try_make(value_type const value) noexcept
        -> std::optional<ConstrainedValue> {
        if (!valid(value)) {
            return std::nullopt;
        }
        return ConstrainedValue{value, Validated{}};
    }

    [[nodiscard]] constexpr auto get() const noexcept -> value_type { return value_; }
    [[nodiscard]] constexpr operator value_type() const noexcept { return value_; }
    auto operator==(ConstrainedValue const&) const -> bool = default;
  private:
    struct Validated {};
    explicit constexpr ConstrainedValue(value_type const value, Validated) noexcept
        : value_{value} {}

    value_type value_{};
};
}

template <typename T, ioj::ConstrainedValuePolicy policy, typename Char>
    requires std::formattable<T, Char>
struct std::formatter<ioj::ConstrainedValue<T, policy>, Char> : std::formatter<T, Char> {
    template <typename FormatContext>
    auto format(ioj::ConstrainedValue<T, policy> const value, FormatContext& context) const ->
        typename FormatContext::iterator {
        return std::formatter<T, Char>::format(value.get(), context);
    }
};
