#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace codegen {

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

struct FixedPointSchema {
    std::string name;
    bool signedness{};
    std::uint32_t total_bits{};
    std::uint32_t fractional_bits{};
    FixedPointRounding rounding{FixedPointRounding::nearest_even};
};

} // namespace codegen
