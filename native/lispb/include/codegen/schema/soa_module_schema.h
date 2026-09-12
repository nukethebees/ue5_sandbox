#pragma once

#include <codegen/schema/module_settings.h>
#include <codegen/schema/soa_schema.h>

#include <vector>

namespace codegen {

struct SoaAllocatorVariant {
    std::string prefix;
    TypeRef allocator;
};

struct SoaModuleSchema {
    ModuleSettings settings;
    std::vector<SoaSchema> structs;
    bool experimental_stdlib{false};
    std::vector<SoaAllocatorVariant> experimental_array_allocators;
};

} // namespace codegen
