#pragma once

#include <codegen/schema/fixed_soa_schema.h>
#include <codegen/schema/module_settings.h>
#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct VectorModuleSchema {
    ModuleSettings settings;
    std::string storage_name;
    TypeRef value_type;
    std::vector<std::string> components;
    TypeRef equivalent_type;
    std::optional<std::string> export_specifier;
    std::optional<FixedSoaSchema> fixed;
};

} // namespace codegen
