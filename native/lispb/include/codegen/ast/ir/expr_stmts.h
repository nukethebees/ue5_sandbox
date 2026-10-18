#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>

#include <vector>

namespace lispb::codegen::ir {

struct ExprStmts {
    std::vector<Expr> expr;
    std::vector<Range> dependencies;
};

} // namespace lispb::codegen::ir
