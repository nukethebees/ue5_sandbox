#pragma once

#include <codegen/ast/syntax.h>
#include <codegen/ast/unary_operator.h>

#include <vector>

namespace lispb::codegen::ir {

struct UnaryExprs {
    std::vector<UnaryOperator> operation;
    std::vector<Syntax> operand;
};

} // namespace lispb::codegen::ir
