#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct EnumeratorSchemas {
    using Indexing = OneBasedIndex<EnumeratorId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> EnumeratorId;
    [[nodiscard]] auto clone(EnumeratorId id) -> EnumeratorId;
    [[nodiscard]] auto index(EnumeratorId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<std::optional<StringId>> initializer;
    Column<std::optional<StringId>> display_name;
    Column<std::uint8_t> hidden;
    Column<std::optional<StringId>> serialized_name;
    Column<std::uint8_t> sentinel;
};

}
