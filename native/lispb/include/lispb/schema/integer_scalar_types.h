#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/schema_range.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {

struct IntegerScalarTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    std::vector<std::uint8_t> signedness;
    std::vector<codegen::PackedIntegerValue> minimum_value;
    std::vector<codegen::PackedIntegerValue> maximum_value;
    std::vector<std::uint32_t> bit_width;
    std::vector<std::uint8_t> bit_width_auto;
    std::vector<codegen::SchemaRange<codegen::PackedNamedCodeId>> named_codes;
    std::vector<RelationshipId> relationship;
    std::vector<ResolvedOccurrenceId> cpp_representation;
};

} // namespace lispb::schema
