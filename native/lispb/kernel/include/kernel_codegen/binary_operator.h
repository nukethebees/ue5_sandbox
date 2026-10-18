#pragma once

#include <cstdint>
namespace kernel_codegen::detail {

enum class BinaryOperator : std::uint8_t { add, subtract, multiply, divide };

auto operator_spelling(BinaryOperator operation) -> char;

}
