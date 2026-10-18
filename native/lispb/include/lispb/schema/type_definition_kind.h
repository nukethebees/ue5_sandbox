#pragma once

#include <cstdint>

namespace lispb::schema {

enum class TypeDefinitionKind : std::uint8_t {
    external,
    enumeration,
    integer_scalar,
    linear_quantized,
    integer_varint,
    fixed_point,
    mini_float,
    optional_sentinel,
    optional_presence_bit,
    packed,
    record,
    union_type,
    tagged_union,
    soa,
    static_table,
    facade,
    homogeneous_storage,
};

} // namespace lispb::schema
