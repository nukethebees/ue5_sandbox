#pragma once

#include <codegen/schema/type_ref.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace codegen {

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

struct IntegerVarintSchema {
    std::string name;
    TypeRef source;
    IntegerVarintEncoding encoding{IntegerVarintEncoding::unsigned_varint};
};

} // namespace codegen
