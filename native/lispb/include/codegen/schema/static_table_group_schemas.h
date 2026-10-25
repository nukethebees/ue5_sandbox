#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

namespace lispb::codegen {

struct StaticTableGroupSchemas {
    using Indexing = OneBasedIndex<StaticTableGroupId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> StaticTableGroupId;
    [[nodiscard]] auto clone(StaticTableGroupId id) -> StaticTableGroupId;
    [[nodiscard]] auto index(StaticTableGroupId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<TypeRefId> type;
    Column<SchemaRange<StringId>> columns;
};

}
