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
    record,
    union_type,
    soa,
    static_table,
    facade,
};

} // namespace lispb::schema
