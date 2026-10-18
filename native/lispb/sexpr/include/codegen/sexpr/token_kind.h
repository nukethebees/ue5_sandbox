#pragma once

#include <cstdint>
namespace lispb::codegen::sexpr {

enum class TokenKind : std::uint8_t {
    atom,
    keyword,
    string,
    raw_literal,
    left_parenthesis,
    right_parenthesis,
    end,
};

}
