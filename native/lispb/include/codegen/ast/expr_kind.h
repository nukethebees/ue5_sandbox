#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class ExprKind : std::uint8_t {
    invalid,
    named,
    literal,
    string_literal,
    call,
    member_access,
    subscript,
    binary,
    unary,
    static_cast_,
    sizeof_type,
    initializer_list,
    raw,
    template_id,
    type_reference,
    parenthesized
};

} // namespace lispb::codegen
