#pragma once

#include <codegen/ast/cpp_type.h>

#include <optional>
#include <string>

namespace codegen {

struct FunctionParameter {
    CppType type;
    std::string name;
    std::optional<std::string> default_value;

    FunctionParameter(CppType value_type, std::string value_name);
    FunctionParameter(CppType value_type, std::string value_name, std::string value_default);
};

} // namespace codegen
