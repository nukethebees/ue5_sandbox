#pragma once

#include <codegen/ast/expr.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct ReturnStmt {
    std::optional<Expr> expression;
    std::vector<TypeDependency> dependencies;
};

} // namespace codegen
