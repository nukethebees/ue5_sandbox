#pragma once

#include <codegen/ast/syntax.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct MemberInitializers {
    std::vector<StringId> member;
    std::vector<Syntax> value;
};

} // namespace lispb::codegen::ir
