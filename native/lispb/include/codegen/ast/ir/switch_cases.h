#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>

#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct SwitchCases {
    std::vector<std::optional<Expr>> label;
    std::vector<Range> body;
    std::vector<bool> braced;
};

} // namespace lispb::codegen::ir
