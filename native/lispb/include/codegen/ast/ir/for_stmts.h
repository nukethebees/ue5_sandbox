#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/node_id.h>

#include <vector>

namespace lispb::codegen::ir {

struct ForStmts {
    std::vector<NodeId> initializer;
    std::vector<Expr> condition;
    std::vector<Expr> increment;
    std::vector<Range> children;
};

} // namespace lispb::codegen::ir
