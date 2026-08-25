#pragma once

#include <codegen/ast/cpp_type.h>

#include <optional>
#include <string>

namespace codegen {

struct FunctionParameter {
    CppType type;
    std::string name;
    std::optional<std::string> default_value;

    FunctionParameter(CppType type, std::string name);
    FunctionParameter(CppType type, std::string name, std::string default_value);
};

} // namespace codegen
