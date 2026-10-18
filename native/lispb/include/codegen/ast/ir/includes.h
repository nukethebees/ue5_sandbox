#pragma once

#include <codegen/ast/include_style.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct Includes {
    std::vector<StringId> path;
    std::vector<IncludeStyle> style;
};

} // namespace lispb::codegen::ir
