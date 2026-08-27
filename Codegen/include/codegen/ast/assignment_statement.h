#pragma once

#include <codegen/ast/type_dependency.h>

#include <string>
#include <vector>

namespace codegen {

struct AssignmentStatement {
    std::string target;
    std::string value;
    std::vector<TypeDependency> dependencies;
};

} // namespace codegen
