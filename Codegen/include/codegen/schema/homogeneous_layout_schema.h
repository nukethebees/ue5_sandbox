#pragma once

#include <codegen/schema/homogeneous_value_schema.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct HomogeneousLayoutSchema {
    std::string name;
    std::vector<std::string> components;
    std::vector<HomogeneousValueSchema> value_types;
    std::optional<std::string> export_specifier;
};

} // namespace codegen
