#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class ExprKind : std::uint8_t {
    invalid,
    named,
    literal,
    boolean_literal,
    numeric_literal,
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
