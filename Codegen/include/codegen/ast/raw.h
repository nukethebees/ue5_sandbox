#pragma once

#include <codegen/ast/type_dependency.h>

#include <string>
#include <vector>

namespace codegen {

struct Raw {
    std::string text;
    std::vector<TypeDependency> dependencies;
};

} // namespace codegen
