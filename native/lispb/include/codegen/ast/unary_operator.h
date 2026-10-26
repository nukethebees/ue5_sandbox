#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class UnaryOperator : std::uint8_t {
    logical_not,
    dereference,
    address_of,
    preincrement,
    bitwise_not,
    negate,
};

} // namespace lispb::codegen
