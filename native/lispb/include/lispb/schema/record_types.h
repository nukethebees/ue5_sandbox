#pragma once

#include <codegen/schema/layout_kind.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/semantic_range.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lispb::schema {

struct RecordTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::record>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    [[nodiscard]] auto members_of(TypeDefinitionId id) const -> std::span<ChildOccurrenceId const>;
    void set_members(TypeDefinitionId id, std::span<ChildOccurrenceId const> values);

    IndexedColumn<codegen::LayoutKind, Index> layout;
    IndexedColumn<std::optional<std::uint32_t>, Index> bits;
    IndexedColumn<ResolvedOccurrenceId, Index> storage_type;
    IndexedColumn<std::optional<std::uint64_t>, Index> invalid_raw_value;
    IndexedColumn<std::optional<codegen::PackedByteOrder>, Index> byte_order;
    IndexedColumn<codegen::PackedBitOrder, Index> bit_order;
    IndexedColumn<SemanticRange, Index> members;
    std::vector<ChildOccurrenceId> members_values;
};

} // namespace lispb::schema
