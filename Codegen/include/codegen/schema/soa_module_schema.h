#pragma once

#include <codegen/schema/module_settings.h>
#include <codegen/schema/soa_schema.h>

#include <vector>

namespace codegen {

struct SoaModuleSchema {
    ModuleSettings settings;
    std::vector<SoaSchema> structs;
};

} // namespace codegen
