#pragma once

#include <codegen/ast/expr.h>

#include <string>

namespace codegen {

struct VariableDeclarationStmt {
    CppType type;
    std::string name;
    Expr initializer;
};

} // namespace codegen
