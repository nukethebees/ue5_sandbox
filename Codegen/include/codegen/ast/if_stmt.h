#pragma once

#include <codegen/ast/block.h>
#include <codegen/ast/expr.h>

namespace codegen {

struct IfStmt {
    Expr condition;
    Block then_block;
    std::optional<Block> else_block;
};

} // namespace codegen
