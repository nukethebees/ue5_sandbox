#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/packed_segment_id.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {

struct PackedFields {
    [[nodiscard]] auto append() -> PackedSegmentId;
    [[nodiscard]] auto index(PackedSegmentId id) const -> std::uint32_t;

    std::vector<codegen::PackedSegmentId> source;
    std::vector<StringId> name;
    std::vector<ResolvedOccurrenceId> semantic_type;
    std::vector<std::uint32_t> bit_width;
    std::vector<std::uint8_t> bit_width_auto;
    std::vector<codegen::PackedFieldKind> kind;
    std::vector<std::uint8_t> range_helper;
    std::vector<std::optional<codegen::PackedIntegerValue>> minimum_value;
    std::vector<std::optional<codegen::PackedIntegerValue>> maximum_value;
    std::vector<codegen::SchemaRange<codegen::PackedNamedCodeId>> named_codes;
    std::vector<RelationshipId> relationship;
    std::vector<std::optional<codegen::PackedIntegerValue>> default_value;
};

} // namespace lispb::schema
