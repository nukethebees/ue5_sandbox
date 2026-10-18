#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>

#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct Returns {
    std::vector<std::optional<Expr>> expression;
    std::vector<Range> dependencies;
};

} // namespace lispb::codegen::ir
