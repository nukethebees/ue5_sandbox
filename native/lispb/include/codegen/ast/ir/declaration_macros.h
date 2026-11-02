#pragma once

#include <codegen/ast/expr.h>

#include <vector>

namespace lispb::codegen::ir {

struct DeclarationMacros {
    std::vector<Expr> invocation;
};

} // namespace lispb::codegen::ir
