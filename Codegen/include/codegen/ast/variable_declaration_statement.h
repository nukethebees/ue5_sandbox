#pragma once

#include <codegen/ast/cpp_type.h>

#include <string>

namespace codegen {

struct VariableDeclarationStatement {
    CppType type;
    std::string name;
    std::string initializer;
};

} // namespace codegen
