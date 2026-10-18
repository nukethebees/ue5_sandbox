#pragma once

#include <codegen/ast/ir/range.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct RawExpressions {
    std::vector<StringId> text;
    std::vector<Range> dependencies;
};

} // namespace lispb::codegen::ir
