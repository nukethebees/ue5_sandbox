#pragma once

#include <codegen/ast/block.h>
#include <codegen/ast/expr.h>

namespace codegen {

struct SwitchCase {
    std::optional<Expr> label;
    Block body;
};

struct SwitchStatement {
    Expr condition;
    std::vector<SwitchCase> cases;
};

} // namespace codegen
