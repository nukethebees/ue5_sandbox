#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct TaggedUnionAlternativeSchemas {
    using Indexing = OneBasedIndex<TaggedUnionAlternativeId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> TaggedUnionAlternativeId;
    [[nodiscard]] auto clone(TaggedUnionAlternativeId id) -> TaggedUnionAlternativeId;
    [[nodiscard]] auto index(TaggedUnionAlternativeId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<TypeRefId> type;
    Column<std::optional<std::uint64_t>> count;
    Column<StringId> tag;
};

}
