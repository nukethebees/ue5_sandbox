#pragma once

#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/packed_integer_value.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>

namespace lispb::schema {

struct FixedPointTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::fixed_point>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    IndexedColumn<std::uint8_t, Index> signedness;
    IndexedColumn<std::uint32_t, Index> total_bits;
    IndexedColumn<std::uint32_t, Index> fractional_bits;
    IndexedColumn<codegen::FixedPointRounding, Index> rounding;
    IndexedColumn<std::optional<codegen::PackedIntegerValue>, Index> minimum_raw_value;
    IndexedColumn<std::optional<codegen::PackedIntegerValue>, Index> maximum_raw_value;
};

} // namespace lispb::schema
