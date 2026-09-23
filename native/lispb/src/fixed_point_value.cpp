#include <codegen/schema/fixed_point_value.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace codegen {

auto parse_fixed_point_value(std::string_view const text, std::uint32_t const fractional_bits)
    -> std::expected<PackedIntegerValue, std::string> {
    if (text.empty() || fractional_bits > 64) {
        return std::unexpected{"Expected a finite decimal fixed-point value."};
    }

    auto cursor{std::size_t{0}};
    auto const negative{text[cursor] == '-'};
    if (negative || text[cursor] == '+') {
        ++cursor;
    }
    auto whole{std::uint64_t{0}};
    auto digits{std::size_t{0}};
    while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9') {
        auto const digit{static_cast<std::uint64_t>(text[cursor] - '0')};
        if (whole > ((std::numeric_limits<std::uint64_t>::max)() - digit) / 10) {
            return std::unexpected{"Fixed-point value exceeds the supported 64-bit range."};
        }
        whole = whole * 10 + digit;
        ++cursor;
        ++digits;
    }

    std::vector<std::uint8_t> fraction;
    if (cursor < text.size() && text[cursor] == '.') {
        ++cursor;
        while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9') {
            fraction.push_back(static_cast<std::uint8_t>(text[cursor] - '0'));
            ++cursor;
            ++digits;
        }
    }
    if (digits == 0 || cursor != text.size()) {
        return std::unexpected{"Expected a finite decimal fixed-point value."};
    }
    if (whole != 0 && (fractional_bits == 64 ||
                       whole > (std::numeric_limits<std::uint64_t>::max)() >> fractional_bits)) {
        return std::unexpected{"Fixed-point value exceeds the supported 64-bit range."};
    }

    auto fractional_raw{std::uint64_t{0}};
    for (auto bit{std::uint32_t{0}}; bit < fractional_bits; ++bit) {
        auto carry{std::uint8_t{0}};
        for (auto index{fraction.size()}; index > 0; --index) {
            auto const doubled{static_cast<std::uint8_t>(fraction[index - 1] * 2 + carry)};
            fraction[index - 1] = doubled % 10;
            carry = doubled / 10;
        }
        fractional_raw = (fractional_raw << 1) | carry;
    }
    if (std::ranges::any_of(fraction, [](auto const digit) { return digit != 0; })) {
        return std::unexpected{"Fixed-point bound must align with the fractional-bit resolution."};
    }

    auto const whole_raw{fractional_bits == 64 ? std::uint64_t{0} : whole << fractional_bits};
    auto const magnitude{whole_raw | fractional_raw};
    return PackedIntegerValue::from_parts(negative, magnitude);
}

} // namespace codegen
