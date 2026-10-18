#pragma once

#include <cstdint>
#include <string_view>

namespace lispb::codegen {

enum class QuantizationClipping : std::uint8_t {
    reject,
    clamp,
};

[[nodiscard]] constexpr auto quantization_clipping_name(QuantizationClipping const clipping)
    -> std::string_view {
    switch (clipping) {
        case QuantizationClipping::reject:
            return "reject";
        case QuantizationClipping::clamp:
            return "clamp";
    }
    return "unknown";
}

} // namespace lispb::codegen
