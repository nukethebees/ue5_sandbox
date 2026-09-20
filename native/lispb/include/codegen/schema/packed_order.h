#pragma once

#include <string_view>

namespace codegen {

enum class PackedByteOrder {
    little_endian,
    big_endian,
};

enum class PackedBitOrder {
    least_significant_first,
    most_significant_first,
};

inline constexpr auto packed_byte_order_name(PackedByteOrder const order) -> std::string_view {
    switch (order) {
        case PackedByteOrder::little_endian:
            return "little";
        case PackedByteOrder::big_endian:
            return "big";
    }
    return "unknown";
}

inline constexpr auto packed_bit_order_name(PackedBitOrder const order) -> std::string_view {
    switch (order) {
        case PackedBitOrder::least_significant_first:
            return "lsb-first";
        case PackedBitOrder::most_significant_first:
            return "msb-first";
    }
    return "unknown";
}

} // namespace codegen
