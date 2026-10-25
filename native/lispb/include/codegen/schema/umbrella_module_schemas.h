#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_kind.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

namespace lispb::codegen {

struct UmbrellaModuleSchemas {
    using Indexing = TaggedIndex<ModuleHandle, ModuleKind::umbrella>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> ModuleHandle;
    [[nodiscard]] auto clone(ModuleHandle id) -> ModuleHandle;
    [[nodiscard]] auto index(ModuleHandle id) const -> std::uint32_t;
    void clear();

    Column<ModuleSettingsId> settings;
    Column<SchemaRange<StringId>> headers;
};

}
