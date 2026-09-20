#pragma once

#include <codegen/schema/integer_scalar_schema.h>
#include <codegen/schema/module_settings.h>

#include <vector>

namespace codegen {

struct ScalarModuleSchema {
    ModuleSettings settings;
    std::vector<IntegerScalarSchema> scalars;
};

} // namespace codegen
