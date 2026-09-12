#pragma once

#include <codegen/schema/homogeneous_layout_schema.h>
#include <codegen/schema/module_settings.h>

#include <vector>

namespace codegen {

struct HomogeneousModuleSchema {
    ModuleSettings settings;
    std::vector<HomogeneousLayoutSchema> layouts;
};

} // namespace codegen
