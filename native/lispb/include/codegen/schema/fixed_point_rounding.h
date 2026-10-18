#pragma once

#include <cstdint>
#include <string_view>

namespace lispb::codegen {

enum class FixedPointRounding : std::uint8_t {
    nearest_even,
    toward_zero,
};

[[nodiscard]] constexpr auto fixed_point_rounding_name(FixedPointRounding const rounding)
    -> std::string_view {
    switch (rounding) {
        case FixedPointRounding::nearest_even:
            return "nearest-even";
        case FixedPointRounding::toward_zero:
            return "toward-zero";
    }
    return "unknown";
}

} // namespace lispb::codegen
