#pragma once

#include <codegen/ast/expr.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct StaticAssertions {
    std::vector<Expr> condition;
    std::vector<StringId> message;
};

} // namespace lispb::codegen::ir
