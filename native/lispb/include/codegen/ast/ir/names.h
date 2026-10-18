#pragma once

#include <codegen/ast/ir/range.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen::ir {

struct Names {
    std::vector<StringId> spelling;
    std::vector<Range> dependencies;
};

} // namespace lispb::codegen::ir
