#pragma once

#include <codegen/ast/expr.h>

#include <vector>

namespace lispb::codegen::ir {

struct ConditionalExprs {
    std::vector<Expr> condition;
    std::vector<Expr> when_true;
    std::vector<Expr> when_false;
};

} // namespace lispb::codegen::ir
