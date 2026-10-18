#pragma once

#include <cstdint>
#include <string_view>

namespace lispb::codegen {

enum class IntegerVarintEncoding : std::uint8_t {
    unsigned_varint,
    signed_varint,
    zigzag_varint,
};

[[nodiscard]] constexpr auto integer_varint_encoding_name(IntegerVarintEncoding const encoding)
    -> std::string_view {
    switch (encoding) {
        case IntegerVarintEncoding::unsigned_varint:
            return "unsigned";
        case IntegerVarintEncoding::signed_varint:
            return "signed";
        case IntegerVarintEncoding::zigzag_varint:
            return "zigzag";
    }
    return "unknown";
}

} // namespace lispb::codegen
