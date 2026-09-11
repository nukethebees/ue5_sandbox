#pragma once

#include <codegen/ast/expr.h>

#include <string>
#include <vector>

namespace codegen {

struct ExpressionStmt {
    Expr expression;
    std::vector<TypeDependency> dependencies;
};

} // namespace codegen
