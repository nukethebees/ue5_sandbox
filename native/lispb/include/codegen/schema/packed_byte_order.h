#pragma once

#include <cstdint>
#include <string_view>

namespace lispb::codegen {

enum class PackedByteOrder : std::uint8_t {
    little_endian,
    big_endian,
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

} // namespace lispb::codegen
