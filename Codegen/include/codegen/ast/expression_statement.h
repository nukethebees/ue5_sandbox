#pragma once

#include <codegen/ast/type_dependency.h>

#include <string>
#include <vector>

namespace codegen {

struct ExpressionStatement {
    std::string expression;
    std::vector<TypeDependency> dependencies;
};

} // namespace codegen
