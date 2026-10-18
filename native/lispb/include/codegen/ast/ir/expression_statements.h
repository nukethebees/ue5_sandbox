#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>

#include <vector>

namespace lispb::codegen::ir {

struct ExpressionStatements {
    std::vector<Expr> expression;
    std::vector<Range> dependencies;
};

} // namespace lispb::codegen::ir
