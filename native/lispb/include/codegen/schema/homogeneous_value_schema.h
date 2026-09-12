#pragma once

#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct HomogeneousValueSchema {
    TypeRef type;
    std::string suffix;
    std::optional<TypeRef> equivalent_type;
    std::vector<TypeRef> input_types;
};

} // namespace codegen
