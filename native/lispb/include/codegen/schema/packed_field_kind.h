#pragma once

namespace codegen {

enum class PackedFieldKind {
    unsigned_integer,
    signed_integer,
    enumeration,
    linear_quantized,
};

} // namespace codegen
