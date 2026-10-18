#pragma once

#include <codegen/ast/expr.h>

#include <vector>

namespace lispb::codegen::ir {

struct Subscripts {
    std::vector<Expr> object;
    std::vector<Expr> index;
};

} // namespace lispb::codegen::ir
