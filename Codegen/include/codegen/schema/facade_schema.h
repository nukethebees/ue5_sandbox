#pragma once

#include <codegen/schema/facade_method_schema.h>
#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct FacadeSchema {
    std::string name;
    TypeRef target_type;
    std::string target_member_name;
    std::vector<FacadeMethodSchema> methods;
    std::vector<std::string> validation_lines;
    std::vector<std::string> validation_dependencies;
    std::optional<std::string> export_specifier;
    std::string bind_access{"public"};
    std::string method_access{"public"};
    std::vector<std::string> friends;
    bool definitions_in_source{false};
};

} // namespace codegen
