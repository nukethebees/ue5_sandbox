#pragma once

namespace codegen {

enum class PackedFieldKind {
    unsigned_integer,
    signed_integer,
    enumeration,
    linear_quantized,
    fixed_point,
    mini_float,
};

} // namespace codegen
