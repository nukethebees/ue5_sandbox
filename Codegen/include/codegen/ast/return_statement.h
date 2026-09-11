#pragma once

#include <codegen/ast/expr.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct ReturnStatement {
    std::optional<Expr> expression;
    std::vector<TypeDependency> dependencies;
};

} // namespace codegen
