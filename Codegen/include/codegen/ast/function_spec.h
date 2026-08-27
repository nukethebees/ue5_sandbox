#pragma once

#include <codegen/ast/cpp_type.h>
#include <codegen/ast/function_parameter.h>
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
    std::string suffix;
    bool is_static{false};
    bool is_inline{false};
    std::optional<std::string> template_parameters;
    std::optional<std::string> requires_clause;
    bool compact_body{false};
    bool requires_on_new_line{true};
    bool requires_before_signature{false};
    bool opening_brace_on_new_line{false};
    bool template_on_same_line{false};
};

} // namespace codegen
