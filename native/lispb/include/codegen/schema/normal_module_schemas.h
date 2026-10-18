#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_kind.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/soa_backend.h>
#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen {

struct NormalModuleSchemas {
    [[nodiscard]] auto append() -> ModuleHandle;
    [[nodiscard]] auto clone(ModuleHandle id) -> ModuleHandle;
    [[nodiscard]] auto index(ModuleHandle id) const -> std::uint32_t;
    void clear();

    std::vector<ModuleSettingsId> settings;
    std::vector<SchemaRange<DeclarationHandle>> declarations;
    std::vector<std::optional<IdentifierId>> enum_helper_namespace;
    std::vector<SoaBackend> soa_backend;
    std::vector<SchemaRange<SoaAllocatorVariantId>> soa_array_allocators;
};

}
