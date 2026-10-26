#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>

namespace lispb::schema {

struct IntegerScalarTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::integer_scalar>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    IndexedColumn<std::uint8_t, Index> signedness;
    IndexedColumn<codegen::PackedIntegerValue, Index> minimum_value;
    IndexedColumn<codegen::PackedIntegerValue, Index> maximum_value;
    IndexedColumn<std::uint32_t, Index> bit_width;
    IndexedColumn<std::uint8_t, Index> bit_width_auto;
    IndexedColumn<codegen::SchemaRange<codegen::PackedNamedCodeId>, Index> named_codes;
    IndexedColumn<RelationshipId, Index> relationship;
    IndexedColumn<ResolvedOccurrenceId, Index> cpp_representation;
};

} // namespace lispb::schema
