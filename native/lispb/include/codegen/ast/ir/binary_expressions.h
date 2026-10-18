#pragma once

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/expr.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen::ir {

struct BinaryExpressions {
    std::vector<BinaryOperator> operation;
    std::vector<Expr> left;
    std::vector<Expr> right;
    std::vector<std::uint8_t> line_break_after;
};

} // namespace lispb::codegen::ir
