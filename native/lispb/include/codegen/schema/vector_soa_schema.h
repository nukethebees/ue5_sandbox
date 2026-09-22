#pragma once

#include <codegen/schema/fixed_soa_schema.h>
#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct VectorSoaSchema {
    std::string name;
    TypeRef value_type;
    std::vector<std::string> components;
    std::vector<std::string> equivalent_members;
    std::optional<std::string> equivalent_constructor;
    TypeRef equivalent_type;
    std::optional<std::string> export_specifier;
    std::optional<FixedSoaSchema> fixed;
};

} // namespace codegen
