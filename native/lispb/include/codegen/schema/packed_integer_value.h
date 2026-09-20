#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

namespace codegen {

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

[[nodiscard]] inline auto packed_integer_less(PackedIntegerValue const left,
                                              PackedIntegerValue const right) -> bool {
    if (left.negative != right.negative) {
        return left.negative;
    }
    return left.negative ? left.magnitude > right.magnitude : left.magnitude < right.magnitude;
}

[[nodiscard]] inline auto packed_integer_less_equal(PackedIntegerValue const left,
                                                    PackedIntegerValue const right) -> bool {
    return left == right || packed_integer_less(left, right);
}

[[nodiscard]] inline auto format_packed_integer(PackedIntegerValue const value) -> std::string {
    return (value.negative ? "-" : "") + std::to_string(value.magnitude);
}

[[nodiscard]] inline auto packed_integer_as_unsigned(PackedIntegerValue const value)
    -> std::optional<std::uint64_t> {
    return value.negative ? std::nullopt : std::optional<std::uint64_t>{value.magnitude};
}

[[nodiscard]] inline auto packed_integer_as_signed(PackedIntegerValue const value)
    -> std::optional<std::int64_t> {
    constexpr auto minimum_magnitude{std::uint64_t{1} << 63};
    if (!value.negative) {
        if (value.magnitude >
            static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(value.magnitude);
    }
    if (value.magnitude > minimum_magnitude) {
        return std::nullopt;
    }
    if (value.magnitude == minimum_magnitude) {
        return (std::numeric_limits<std::int64_t>::min)();
    }
    return -static_cast<std::int64_t>(value.magnitude);
}

[[nodiscard]] inline auto packed_integer_fits_unsigned(PackedIntegerValue const value,
                                                       std::uint32_t const bits) -> bool {
    if (value.negative || bits == 0 || bits > 64) {
        return false;
    }
    return bits == 64 || value.magnitude < (std::uint64_t{1} << bits);
}

[[nodiscard]] inline auto packed_integer_fits_signed(PackedIntegerValue const value,
                                                     std::uint32_t const bits) -> bool {
    if (bits == 0 || bits > 64) {
        return false;
    }
    auto const negative_limit{bits == 64 ? (std::uint64_t{1} << 63)
                                         : (std::uint64_t{1} << (bits - 1))};
    return value.negative ? value.magnitude <= negative_limit : value.magnitude < negative_limit;
}

[[nodiscard]] inline auto minimum_packed_integer_bits(PackedIntegerValue const minimum,
                                                      PackedIntegerValue const maximum,
                                                      bool const signed_domain)
    -> std::optional<std::uint32_t> {
    if (packed_integer_less(maximum, minimum)) {
        return std::nullopt;
    }
    for (auto bits{std::uint32_t{1}}; bits <= 64; ++bits) {
        auto const minimum_fits{signed_domain ? packed_integer_fits_signed(minimum, bits)
                                              : packed_integer_fits_unsigned(minimum, bits)};
        auto const maximum_fits{signed_domain ? packed_integer_fits_signed(maximum, bits)
                                              : packed_integer_fits_unsigned(maximum, bits)};
        if (minimum_fits && maximum_fits) {
            return bits;
        }
    }
    return std::nullopt;
}

} // namespace codegen
