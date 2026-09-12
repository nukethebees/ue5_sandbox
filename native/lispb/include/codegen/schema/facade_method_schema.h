#pragma once

#include <codegen/schema/parameter_schema.h>
#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct FacadeMethodSchema {
    std::string name;
    TypeRef return_type;
    std::vector<ParameterSchema> parameters;
    bool is_const{false};
    bool is_noexcept{false};
    std::optional<std::string> target_name;
};

} // namespace codegen
