#include "varint_encoding.hpp"

#include "analysis_limits.hpp"

#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/packed_integer_value.h>

#include <cstdint>

namespace ioj::layout::analysis_detail {

auto unsigned_varint_bytes(std::uint64_t value) -> std::uint32_t {
    auto bytes{std::uint32_t{1}};
    while (value >= 0x80) {
        value >>= 7;
        ++bytes;
    }
    return bytes;
}

auto signed_integer_value(lispb::codegen::PackedIntegerValue const value) -> std::int64_t {
    if (!value.negative) {
        return static_cast<std::int64_t>(value.magnitude);
    }
    if (value.magnitude == (std::uint64_t{1} << 63)) {
        return minimum_signed_code;
    }
    return -static_cast<std::int64_t>(value.magnitude);
}

auto signed_varint_bytes(std::int64_t value) -> std::uint32_t {
    auto bytes{std::uint32_t{0}};
    while (true) {
        auto const encoded{static_cast<std::uint8_t>(value & 0x7f)};
        auto const sign_bit_set{(encoded & 0x40) != 0};
        value >>= 7;
        ++bytes;
        if ((value == 0 && !sign_bit_set) || (value == -1 && sign_bit_set)) {
            return bytes;
        }
    }
}

auto zigzag_value(lispb::codegen::PackedIntegerValue const value) -> std::uint64_t {
    return value.negative ? value.magnitude * 2 - 1 : value.magnitude * 2;
}

auto integer_varint_bytes(lispb::codegen::PackedIntegerValue const value,
                          lispb::codegen::IntegerVarintEncoding const encoding) -> std::uint32_t {
    switch (encoding) {
        case lispb::codegen::IntegerVarintEncoding::unsigned_varint:
            return unsigned_varint_bytes(value.magnitude);
        case lispb::codegen::IntegerVarintEncoding::signed_varint:
            return signed_varint_bytes(signed_integer_value(value));
        case lispb::codegen::IntegerVarintEncoding::zigzag_varint:
            return unsigned_varint_bytes(zigzag_value(value));
    }
    return 0;
}

}
