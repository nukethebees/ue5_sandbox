#pragma once

#include <codegen/ast/expr.h>

#include <string>
#include <vector>

namespace codegen {

struct AssignmentStatement {
    Expr target;
    Expr value;
    std::vector<TypeDependency> dependencies;
};

} // namespace codegen
