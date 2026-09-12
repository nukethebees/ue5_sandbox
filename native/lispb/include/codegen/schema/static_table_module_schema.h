#pragma once

#include <codegen/schema/module_settings.h>
#include <codegen/schema/static_table_schema.h>

#include <vector>

namespace codegen {

struct StaticTableModuleSchema {
    ModuleSettings settings;
    std::vector<StaticTableSchema> tables;
};

} // namespace codegen
