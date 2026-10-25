#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

namespace lispb::codegen {

struct StaticTableRowSchemas {
    using Indexing = OneBasedIndex<StaticTableRowId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> StaticTableRowId;
    [[nodiscard]] auto clone(StaticTableRowId id) -> StaticTableRowId;
    [[nodiscard]] auto index(StaticTableRowId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
};

}
