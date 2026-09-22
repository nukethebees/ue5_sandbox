#pragma once

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/module_settings.h>
#include <codegen/schema/soa_module_schema.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct NormalModuleSchema {
    ModuleSettings settings;
    std::vector<DeclarationSchema> declarations;
    std::optional<std::string> enum_helper_namespace;
    SoaBackend soa_backend{SoaBackend::unreal};
    std::vector<SoaAllocatorVariant> soa_array_allocators;
};

} // namespace codegen
