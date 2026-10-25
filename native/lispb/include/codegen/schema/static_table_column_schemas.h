#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

namespace lispb::codegen {

struct StaticTableColumnSchemas {
    using Indexing = OneBasedIndex<StaticTableColumnId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> StaticTableColumnId;
    [[nodiscard]] auto clone(StaticTableColumnId id) -> StaticTableColumnId;
    [[nodiscard]] auto index(StaticTableColumnId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<TypeRefId> type;
};

}
