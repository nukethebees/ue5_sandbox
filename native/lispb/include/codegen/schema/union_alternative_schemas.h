#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct UnionAlternativeSchemas {
    using Indexing = OneBasedIndex<UnionAlternativeId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> UnionAlternativeId;
    [[nodiscard]] auto clone(UnionAlternativeId id) -> UnionAlternativeId;
    [[nodiscard]] auto index(UnionAlternativeId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<TypeRefId> type;
    Column<std::optional<std::uint64_t>> count;
};

}
