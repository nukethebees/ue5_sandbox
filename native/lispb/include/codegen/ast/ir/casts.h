#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct Casts {
    std::vector<CppTypeId> type;
    std::vector<Expr> operand;
};

} // namespace lispb::codegen::ir
