#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <optional>

namespace lispb::codegen {

struct ModuleSettingsStore {
    using Indexing = OneBasedIndex<ModuleSettingsId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> ModuleSettingsId;
    [[nodiscard]] auto clone(ModuleSettingsId id) -> ModuleSettingsId;
    [[nodiscard]] auto index(ModuleSettingsId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<StringId> header;
    Column<std::optional<StringId>> source;
    Column<std::optional<StringId>> header_include;
    Column<std::optional<StringId>> namespace_name;
    Column<SchemaRange<StringId>> include_order;
    Column<SchemaRange<StringId>> prelude_lines;
};

}
