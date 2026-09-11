#pragma once

#include <codegen/ast/expr.h>

#include <string>
#include <vector>

namespace codegen {

struct AssignmentStmt {
    Expr target;
    Expr value;
    std::vector<TypeDependency> dependencies;
};

} // namespace codegen
