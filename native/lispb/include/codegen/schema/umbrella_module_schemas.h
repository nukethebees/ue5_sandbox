#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_kind.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct UmbrellaModuleSchemas {
    [[nodiscard]] auto append() -> ModuleHandle;
    [[nodiscard]] auto clone(ModuleHandle id) -> ModuleHandle;
    [[nodiscard]] auto index(ModuleHandle id) const -> std::uint32_t;
    void clear();

    std::vector<ModuleSettingsId> settings;
    std::vector<SchemaRange<StringId>> headers;
};

}
