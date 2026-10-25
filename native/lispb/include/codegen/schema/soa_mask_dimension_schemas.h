#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

namespace lispb::codegen {

struct SoaMaskDimensionSchemas {
    using Indexing = OneBasedIndex<SoaMaskDimensionId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> SoaMaskDimensionId;
    [[nodiscard]] auto clone(SoaMaskDimensionId id) -> SoaMaskDimensionId;
    [[nodiscard]] auto index(SoaMaskDimensionId id) const -> std::uint32_t;
    void clear();

    Column<StringId> index_name;
    Column<StringId> extent;
};

}
