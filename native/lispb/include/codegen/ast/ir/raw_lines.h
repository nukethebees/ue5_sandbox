#pragma once

#include <codegen/ast/ir/range.h>

#include <vector>

namespace lispb::codegen::ir {

struct RawLines {
    std::vector<Range> lines;
    std::vector<Range> dependencies;
};

} // namespace lispb::codegen::ir
