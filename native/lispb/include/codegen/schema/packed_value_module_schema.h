#pragma once

#include <codegen/schema/module_settings.h>
#include <codegen/schema/packed_value_schema.h>

#include <vector>

namespace codegen {

struct PackedValueModuleSchema {
    ModuleSettings settings;
    std::vector<PackedValueSchema> values;
};

} // namespace codegen
