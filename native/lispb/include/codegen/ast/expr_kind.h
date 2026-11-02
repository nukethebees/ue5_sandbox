#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class ExprKind : std::uint8_t {
    invalid,
    literal,
    boolean_literal,
    i8_literal,
    i16_literal,
    i32_literal,
    i64_literal,
    u8_literal,
    u16_literal,
    u32_literal,
    u64_literal,
    f32_literal,
    f64_literal,
    string_literal,
    call,
    member_access,
    subscript,
    binary,
    conditional,
    lambda,
    unary,
    static_cast_,
    reinterpret_cast_,
    sizeof_type,
    initializer_list,
    raw,
    parenthesized,
    symbol_reference,
    Count
};

} // namespace lispb::codegen
