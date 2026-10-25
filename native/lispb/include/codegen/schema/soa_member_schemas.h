#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/soa_member_kind.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct SoaMemberSchemas {
    using Indexing = OneBasedIndex<SoaMemberId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> SoaMemberId;
    [[nodiscard]] auto clone(SoaMemberId id) -> SoaMemberId;
    [[nodiscard]] auto index(SoaMemberId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<SoaMemberKind> kind;
    Column<TypeRefId> type;
    Column<std::optional<StringId>> equivalent_member;
    Column<std::uint8_t> equivalent_accessor;
    Column<std::uint8_t> mask_field;
    Column<SchemaRange<SoaMaskDimensionId>> mask_dimensions;
    Column<std::optional<SemanticRelationId>> relationship;
};

}
