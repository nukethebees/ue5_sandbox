#pragma once

#include <codegen/schema/parameter_schema.h>
#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct FunctionSchema {
    std::string name;
    TypeRef return_type;
    std::vector<ParameterSchema> parameters;
    std::vector<std::string> body_lines;
    std::vector<std::string> dependencies;
    std::string suffix;
    bool is_static{false};
    bool is_inline{false};
    bool definition_in_source{false};
    std::optional<std::string> template_parameters;
    std::optional<std::string> requires_clause;
};

} // namespace codegen
