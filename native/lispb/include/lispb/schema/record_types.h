#pragma once

#include <codegen/schema/layout_kind.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/semantic_range.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lispb::schema {

struct RecordTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    [[nodiscard]] auto members_of(TypeDefinitionId id) const -> std::span<ChildOccurrenceId const>;
    void set_members(TypeDefinitionId id, std::span<ChildOccurrenceId const> values);

    std::vector<codegen::LayoutKind> layout;
    std::vector<std::optional<std::uint32_t>> bits;
    std::vector<ResolvedOccurrenceId> storage_type;
    std::vector<std::optional<std::uint64_t>> invalid_raw_value;
    std::vector<std::optional<codegen::PackedByteOrder>> byte_order;
    std::vector<codegen::PackedBitOrder> bit_order;
    std::vector<SemanticRange> members;
    std::vector<ChildOccurrenceId> members_values;
};

} // namespace lispb::schema
