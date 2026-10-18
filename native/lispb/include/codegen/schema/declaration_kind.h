#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class DeclarationKind : std::uint8_t {
    enumeration,
    integer_scalar,
    linear_quantized,
    integer_varint,
    fixed_point,
    mini_float,
    optional_sentinel,
    optional_presence_bit,
    packed_value,
    record,
    union_type,
    tagged_union,
    soa,
    vector_soa,
    homogeneous_layout,
    static_table,
    facade,
};

} // namespace lispb::codegen
