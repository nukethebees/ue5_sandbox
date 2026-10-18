#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

namespace lispb::codegen {

struct PackedIntegerValue {
    bool negative{};
    std::uint64_t magnitude{};

    constexpr PackedIntegerValue() = default;

    template <std::integral T>
        requires (!std::same_as<std::remove_cv_t<T>, bool> && sizeof(T) <= sizeof(std::uint64_t))
    constexpr PackedIntegerValue(T const value) {
        if constexpr (std::is_signed_v<T>) {
            negative = value < 0;
            magnitude = negative ? static_cast<std::uint64_t>(-(value + 1)) + std::uint64_t{1}
                                 : static_cast<std::uint64_t>(value);
        } else {
            magnitude = static_cast<std::uint64_t>(value);
        }
    }

    static constexpr auto from_parts(bool const is_negative, std::uint64_t const absolute_value)
        -> PackedIntegerValue {
        auto result{PackedIntegerValue{}};
        result.negative = is_negative && absolute_value != 0;
        result.magnitude = absolute_value;
        return result;
    }

    auto operator==(PackedIntegerValue const&) const -> bool = default;
};

[[nodiscard]] auto packed_integer_less(PackedIntegerValue const left,
                                       PackedIntegerValue const right) -> bool;

[[nodiscard]] auto packed_integer_less_equal(PackedIntegerValue const left,
                                             PackedIntegerValue const right) -> bool;

[[nodiscard]] auto format_packed_integer(PackedIntegerValue const value) -> std::string;

[[nodiscard]] auto packed_integer_as_unsigned(PackedIntegerValue const value)
    -> std::optional<std::uint64_t>;

[[nodiscard]] auto packed_integer_as_signed(PackedIntegerValue const value)
    -> std::optional<std::int64_t>;

[[nodiscard]] auto packed_integer_fits_unsigned(PackedIntegerValue const value,
                                                std::uint32_t const bits) -> bool;

[[nodiscard]] auto packed_integer_fits_signed(PackedIntegerValue const value,
                                              std::uint32_t const bits) -> bool;

[[nodiscard]] auto minimum_packed_integer_bits(PackedIntegerValue const minimum,
                                               PackedIntegerValue const maximum,
                                               bool const signed_domain)
    -> std::optional<std::uint32_t>;

} // namespace lispb::codegen
