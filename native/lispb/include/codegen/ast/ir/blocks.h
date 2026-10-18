#pragma once

#include <codegen/ast/ir/range.h>

#include <vector>

namespace lispb::codegen::ir {

struct Blocks {
    std::vector<Range> children;
};

} // namespace lispb::codegen::ir
