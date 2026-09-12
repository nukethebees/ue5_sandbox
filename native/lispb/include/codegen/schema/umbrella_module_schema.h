#pragma once

#include <codegen/schema/module_settings.h>

#include <string>
#include <vector>

namespace codegen {

struct UmbrellaModuleSchema {
    ModuleSettings settings;
    std::vector<std::string> headers;
};

} // namespace codegen
