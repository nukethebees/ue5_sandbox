#pragma once

#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/packed_segment_id.h>
#include <lispb/schema/semantic_range.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lispb::schema {

struct PackedTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    [[nodiscard]] auto segments_of(TypeDefinitionId id) const -> std::span<PackedSegmentId const>;
    void set_segments(TypeDefinitionId id, std::span<PackedSegmentId const> values);

    std::vector<ResolvedOccurrenceId> storage_type;
    std::vector<SemanticRange> segments;
    std::vector<PackedSegmentId> segments_values;
    std::vector<std::optional<std::uint64_t>> invalid_raw_value;
    std::vector<std::optional<codegen::PackedByteOrder>> byte_order;
    std::vector<codegen::PackedBitOrder> bit_order;
    std::vector<std::optional<std::uint64_t>> default_raw_value;
};

} // namespace lispb::schema
