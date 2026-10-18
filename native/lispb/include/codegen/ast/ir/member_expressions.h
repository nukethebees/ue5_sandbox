#pragma once

#include <codegen/ast/expr.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen::ir {

struct MemberExpressions {
    std::vector<Expr> object;
    std::vector<StringId> member;
    std::vector<std::uint8_t> through_pointer;
};

} // namespace lispb::codegen::ir
