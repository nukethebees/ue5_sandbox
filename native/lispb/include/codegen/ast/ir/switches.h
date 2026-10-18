#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>

#include <vector>

namespace lispb::codegen::ir {

struct Switches {
    std::vector<Expr> condition;
    std::vector<Range> cases;
};

} // namespace lispb::codegen::ir
