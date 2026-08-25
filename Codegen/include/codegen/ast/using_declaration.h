#pragma once

#include <codegen/ast/cpp_type.h>

#include <string>

namespace codegen {

struct UsingDeclaration {
    std::string name;
    CppType type;
};

} // namespace codegen
