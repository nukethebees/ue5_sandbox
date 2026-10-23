#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class BinaryOperator : std::uint8_t {
    equal,
    greater_equal,
    greater,
    subtract,
    add,
    multiply,
    logical_or,
    logical_and,
    not_equal,
    divide,
    less,
    bitwise_and,
    bitwise_or,
    shift_left,
};

} // namespace lispb::codegen
