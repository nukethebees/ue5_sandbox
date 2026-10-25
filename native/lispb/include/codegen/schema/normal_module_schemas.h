#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_kind.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <optional>

namespace lispb::codegen {

struct NormalModuleSchemas {
    using Indexing = TaggedIndex<ModuleHandle, ModuleKind::normal>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> ModuleHandle;
    [[nodiscard]] auto clone(ModuleHandle id) -> ModuleHandle;
    [[nodiscard]] auto index(ModuleHandle id) const -> std::uint32_t;
    void clear();

    Column<ModuleSettingsId> settings;
    Column<SchemaRange<DeclarationHandle>> declarations;
    Column<std::optional<StringId>> enum_helper_namespace;
    Column<std::uint8_t> s7_constructors;
};

}
