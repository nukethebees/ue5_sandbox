#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_segment_kind.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct PackedFieldSchemas {
    [[nodiscard]] auto append() -> PackedSegmentId;
    [[nodiscard]] auto clone(PackedSegmentId id) -> PackedSegmentId;
    [[nodiscard]] auto index(PackedSegmentId id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<TypeRefId> type;
    std::vector<std::optional<int>> bits;
    std::vector<PackedFieldKind> kind;
    std::vector<std::uint8_t> range_helper;
    std::vector<std::optional<PackedIntegerValue>> minimum_value;
    std::vector<std::optional<PackedIntegerValue>> maximum_value;
    std::vector<SchemaRange<PackedNamedCodeId>> named_codes;
    std::vector<std::optional<SemanticRelationId>> relationship;
    std::vector<std::optional<PackedIntegerValue>> default_value;
};

}
