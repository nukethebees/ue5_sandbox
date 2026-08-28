#pragma once

#include <codegen/ast/cpp_type.h>
#include <codegen/ast/function_formatting.h>
#include <codegen/ast/function_parameter.h>
#include <codegen/ast/function_qualifiers.h>
#include <codegen/ast/node_fwd.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct FunctionSpec {
    std::string name;
    CppType return_type;
    std::vector<FunctionParameter> parameters;
    Nodes body;
    FunctionQualifiers qualifiers;
    bool is_static{false};
    bool is_inline{false};
    std::optional<std::string> template_parameters;
    std::optional<std::string> requires_clause;
    FunctionFormatting formatting;
};

} // namespace codegen
