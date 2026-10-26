#pragma once

#include <codegen/ast/expr.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct MemberInitializers {
    std::vector<StringId> member;
    std::vector<Expr> value;
};

} // namespace lispb::codegen::ir
