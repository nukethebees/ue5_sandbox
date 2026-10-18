#pragma once

#include <cstdint>
#include <string_view>

namespace lispb::codegen {

enum class PackedBitOrder : std::uint8_t {
    least_significant_first,
    most_significant_first,
};

inline constexpr auto packed_bit_order_name(PackedBitOrder const order) -> std::string_view {
    switch (order) {
        case PackedBitOrder::least_significant_first:
            return "lsb-first";
        case PackedBitOrder::most_significant_first:
            return "msb-first";
    }
    return "unknown";
}

} // namespace lispb::codegen
