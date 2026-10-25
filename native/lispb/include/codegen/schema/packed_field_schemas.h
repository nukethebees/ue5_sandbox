#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_segment_kind.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct PackedFieldSchemas {
    using Indexing = TaggedIndex<PackedSegmentId, PackedSegmentKind::field>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> PackedSegmentId;
    [[nodiscard]] auto clone(PackedSegmentId id) -> PackedSegmentId;
    [[nodiscard]] auto index(PackedSegmentId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<TypeRefId> type;
    Column<std::optional<int>> bits;
    Column<PackedFieldKind> kind;
    Column<std::uint8_t> range_helper;
    Column<std::optional<PackedIntegerValue>> minimum_value;
    Column<std::optional<PackedIntegerValue>> maximum_value;
    Column<SchemaRange<PackedNamedCodeId>> named_codes;
    Column<std::optional<SemanticRelationId>> relationship;
    Column<std::optional<PackedIntegerValue>> default_value;
};

}
