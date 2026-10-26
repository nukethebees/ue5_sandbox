#pragma once

namespace ioj::layout {
enum class DeclarationKind {
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
    union_,
    soa,
    static_table,
    facade,
};
auto declaration_kind_label(DeclarationKind kind) -> char const*;
}
