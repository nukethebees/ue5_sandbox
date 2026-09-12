#pragma once

#include <codegen/schema/enum_schema.h>
#include <codegen/schema/module_settings.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct EnumModuleSchema {
    ModuleSettings settings;
    std::optional<std::string> helper_namespace;
    std::vector<EnumSchema> enums;
};

} // namespace codegen
