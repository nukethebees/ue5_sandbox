#pragma once

#include <codegen/schema/facade_schema.h>
#include <codegen/schema/module_settings.h>

namespace codegen {

struct FacadeModuleSchema {
    ModuleSettings settings;
    FacadeSchema facade;
};

} // namespace codegen
