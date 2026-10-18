#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>

#include <vector>

namespace lispb::codegen::ir {

struct Assignments {
    std::vector<Expr> target;
    std::vector<Expr> value;
    std::vector<Range> dependencies;
};

} // namespace lispb::codegen::ir
