#pragma once

#include <cstdint>
#include <string_view>

namespace lispb::codegen {

enum class FloatingPointFormat : std::uint8_t {
    ieee754_binary16,
    ieee754_binary32,
    ieee754_binary64
};

constexpr auto floating_point_format_name(FloatingPointFormat const format) -> std::string_view {
    switch (format) {
        case FloatingPointFormat::ieee754_binary16:
            return "ieee754-binary16";
        case FloatingPointFormat::ieee754_binary32:
            return "ieee754-binary32";
        case FloatingPointFormat::ieee754_binary64:
            return "ieee754-binary64";
    }
    return "unknown";
}

} // namespace lispb::codegen
