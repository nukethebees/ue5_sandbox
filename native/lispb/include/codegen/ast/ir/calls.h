#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>

#include <vector>

namespace lispb::codegen::ir {

struct Calls {
    std::vector<Expr> callee;
    std::vector<Range> arguments;
};

} // namespace lispb::codegen::ir
