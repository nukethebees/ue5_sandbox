#pragma once

#include <codegen/ast/ir/range.h>
#include <codegen/ast/syntax.h>

#include <vector>

namespace lispb::codegen::ir {

struct Assignments {
    std::vector<Expr> target;
    std::vector<Syntax> value;
    std::vector<Range> dependencies;
};

} // namespace lispb::codegen::ir
