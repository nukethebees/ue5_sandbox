#pragma once

#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct SoaMaskDimensionSchema {
    std::string index_name;
    std::string extent;
};

struct SoaMemberSchema {
    std::string name;
    SoaMemberKind kind;
    TypeRef type;
    std::optional<std::string> fixed_schema;
    std::optional<std::string> nested_schema;
    bool mask_field{false};
    std::vector<SoaMaskDimensionSchema> mask_dimensions;
};

} // namespace codegen
