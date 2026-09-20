#pragma once

#include <codegen/schema/type_ref.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace codegen {

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

struct LinearQuantizedSchema {
    std::string name;
    TypeRef source;
    std::uint32_t bit_width{};
    std::uint64_t reserved_codes{};
    QuantizationClipping clipping{QuantizationClipping::reject};
};

} // namespace codegen
