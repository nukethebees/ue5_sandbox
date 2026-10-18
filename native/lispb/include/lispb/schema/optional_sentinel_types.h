#pragma once

#include <codegen/schema/packed_integer_value.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <vector>

namespace lispb::schema {

struct OptionalSentinelTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    std::vector<ResolvedOccurrenceId> source;
    std::vector<StringId> sentinel_name;
    std::vector<codegen::PackedIntegerValue> sentinel_value;
    std::vector<std::uint32_t> bit_width;
};

} // namespace lispb::schema
