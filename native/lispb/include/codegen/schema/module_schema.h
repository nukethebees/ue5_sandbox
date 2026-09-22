#pragma once

#include <codegen/schema/module_settings.h>
#include <codegen/schema/normal_module_schema.h>
#include <codegen/schema/settings_module_schema.h>
#include <codegen/schema/umbrella_module_schema.h>

#include <variant>

namespace codegen {

using ModuleSchema = std::variant<NormalModuleSchema, SettingsModuleSchema, UmbrellaModuleSchema>;

} // namespace codegen
