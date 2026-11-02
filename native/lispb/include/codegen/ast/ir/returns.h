#pragma once

#include <codegen/ast/ir/range.h>
#include <codegen/ast/syntax.h>

#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct Returns {
    std::vector<std::optional<Syntax>> expr;
    std::vector<Range> dependencies;
};

} // namespace lispb::codegen::ir
