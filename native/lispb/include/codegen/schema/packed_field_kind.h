#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class PackedFieldKind : std::uint8_t {
    unsigned_integer,
    signed_integer,
    enumeration,
    linear_quantized,
    fixed_point,
    mini_float,
};

} // namespace lispb::codegen
