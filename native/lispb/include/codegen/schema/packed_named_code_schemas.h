#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/packed_integer_value.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>

namespace lispb::codegen {

struct PackedNamedCodeSchemas {
    using Indexing = OneBasedIndex<PackedNamedCodeId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> PackedNamedCodeId;
    [[nodiscard]] auto clone(PackedNamedCodeId id) -> PackedNamedCodeId;
    [[nodiscard]] auto index(PackedNamedCodeId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<PackedIntegerValue> value;
    Column<std::uint8_t> sentinel;
};

}
