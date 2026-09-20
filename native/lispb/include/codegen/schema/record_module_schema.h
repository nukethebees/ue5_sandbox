#pragma once

#include <codegen/schema/module_settings.h>
#include <codegen/schema/record_schema.h>

#include <vector>

namespace codegen {

struct RecordModuleSchema {
    ModuleSettings settings;
    std::vector<RecordSchema> records;
};

} // namespace codegen
