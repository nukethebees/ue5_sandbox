#pragma once

#include <codegen/ast/cpp_type.h>
#include <codegen/ast/node_fwd.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct Struct {
    std::string name;
    Nodes children;
    std::vector<CppType> bases;
    std::optional<std::string> template_parameters;
    std::optional<std::string> requires_clause;
    std::optional<std::string> export_specifier;
    std::string record_kind{"struct"};
    std::vector<TypeDependency> dependencies;
};

} // namespace codegen
