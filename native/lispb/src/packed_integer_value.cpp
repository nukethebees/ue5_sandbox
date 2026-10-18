#include <codegen/schema/packed_integer_value.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace lispb::codegen {

auto packed_integer_less(PackedIntegerValue const left, PackedIntegerValue const right) -> bool {
    if (left.negative != right.negative) {
        return left.negative;
    }
    return left.negative ? left.magnitude > right.magnitude : left.magnitude < right.magnitude;
}
auto packed_integer_less_equal(PackedIntegerValue const left, PackedIntegerValue const right)
    -> bool {
    return left == right || packed_integer_less(left, right);
}
auto format_packed_integer(PackedIntegerValue const value) -> std::string {
    return (value.negative ? "-" : "") + std::to_string(value.magnitude);
}
auto packed_integer_as_unsigned(PackedIntegerValue const value) -> std::optional<std::uint64_t> {
    return value.negative ? std::nullopt : std::optional<std::uint64_t>{value.magnitude};
}
auto packed_integer_as_signed(PackedIntegerValue const value) -> std::optional<std::int64_t> {
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
auto packed_integer_fits_unsigned(PackedIntegerValue const value, std::uint32_t const bits)
    -> bool {
    if (value.negative || bits == 0 || bits > 64) {
        return false;
    }
    return bits == 64 || value.magnitude < (std::uint64_t{1} << bits);
}
auto packed_integer_fits_signed(PackedIntegerValue const value, std::uint32_t const bits) -> bool {
    if (bits == 0 || bits > 64) {
        return false;
    }
    auto const negative_limit{bits == 64 ? (std::uint64_t{1} << 63)
                                         : (std::uint64_t{1} << (bits - 1))};
    return value.negative ? value.magnitude <= negative_limit : value.magnitude < negative_limit;
}
auto minimum_packed_integer_bits(PackedIntegerValue const minimum,
                                 PackedIntegerValue const maximum,
                                 bool const signed_domain) -> std::optional<std::uint32_t> {
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

} // namespace lispb::codegen
