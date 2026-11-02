#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class ExprKind : std::uint8_t {
    invalid,
    named,
    literal,
    boolean_literal,
    int_literal,
    unsigned_int_literal,
    long_literal,
    unsigned_long_literal,
    long_long_literal,
    unsigned_long_long_literal,
    float_literal,
    double_literal,
    long_double_literal,
    string_literal,
    call,
    member_access,
    scope_member,
    subscript,
    binary,
    conditional,
    lambda,
    unary,
    static_cast_,
    reinterpret_cast_,
    sizeof_type,
    initializer_list,
    designated_initializer,
    raw,
    template_id,
    type_reference,
    parenthesized
};

} // namespace lispb::codegen
