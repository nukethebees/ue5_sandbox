#pragma once

#include <codegen/ast/type_dependency.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct ReturnStatement {
    std::optional<std::string> expression;
    std::vector<TypeDependency> dependencies;
};

} // namespace codegen
