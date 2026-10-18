#pragma once

#include <codegen/schema/floating_point_format.h>

#include <cstdint>

namespace lispb::codegen {

struct FloatingPointFormatInfo {
    std::uint32_t exponent_bits;
    std::uint32_t fraction_bits;
    std::int32_t exponent_bias;
};

constexpr auto floating_point_format_info(FloatingPointFormat const format)
    -> FloatingPointFormatInfo {
    switch (format) {
        case FloatingPointFormat::ieee754_binary16:
            return {5, 10, 15};
        case FloatingPointFormat::ieee754_binary32:
            return {8, 23, 127};
        case FloatingPointFormat::ieee754_binary64:
            return {11, 52, 1023};
    }
    return {};
}

} // namespace lispb::codegen
