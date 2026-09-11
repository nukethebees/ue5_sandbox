#pragma once

#include <codegen/ast/expr.h>

#include <string>
#include <vector>

namespace codegen {

struct ExpressionStatement {
    Expr expression;
    std::vector<TypeDependency> dependencies;
};

} // namespace codegen
