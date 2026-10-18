#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>

#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct IfStmts {
    std::vector<Expr> condition;
    std::vector<Range> then_children;
    std::vector<std::optional<Range>> else_children;
};

} // namespace lispb::codegen::ir
