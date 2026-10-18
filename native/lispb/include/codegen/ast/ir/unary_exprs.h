#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/unary_operator.h>

#include <vector>

namespace lispb::codegen::ir {

struct UnaryExprs {
    std::vector<UnaryOperator> operation;
    std::vector<Expr> operand;
};

} // namespace lispb::codegen::ir
