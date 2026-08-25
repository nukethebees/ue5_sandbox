#pragma once

#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>

namespace codegen {

struct ParameterSchema {
    TypeRef type;
    std::string name;
    std::optional<std::string> default_value;
};

} // namespace codegen
