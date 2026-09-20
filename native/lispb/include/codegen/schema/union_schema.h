#pragma once

#include <codegen/schema/union_alternative_schema.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct UnionSchema {
    std::string name;
    std::vector<UnionAlternativeSchema> alternatives;
    std::optional<std::string> export_specifier;
};

} // namespace codegen
