#pragma once

#include <codegen/ast/indices.h>
#include <lispb/schema/packed_segment_id.h>
#include <lispb/schema/type_definition_id.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lispb::codegen {

struct PackedFieldLayouts {
    // Index using an existing semantic packed-field handle after prepare().
    void prepare(std::size_t field_count);
    auto index(schema::PackedSegmentId field) const -> std::uint32_t;
    void clear();

    std::vector<CppTypeId> type;
    std::vector<schema::TypeDefinitionId> definition;
    std::vector<std::uint8_t> boolean;
    std::vector<std::uint32_t> offset;
    std::vector<std::uint64_t> value_mask;
};

}
