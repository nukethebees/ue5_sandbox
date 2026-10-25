#pragma once

#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/indices.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>

namespace lispb::codegen {

struct EnumUnrealProjections {
    using Indexing = OneBasedIndex<EnumUnrealProjectionId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> EnumUnrealProjectionId;
    [[nodiscard]] auto clone(EnumUnrealProjectionId id) -> EnumUnrealProjectionId;
    [[nodiscard]] auto index(EnumUnrealProjectionId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<StringId> header;
    Column<StringId> header_include;
    Column<StringId> conversion_header;
    Column<StringId> native_header_include;
    Column<EnumReflection> reflection;
    Column<std::uint8_t> enum_array;
};

}
