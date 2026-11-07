#pragma once

#include <ioj/min_max.h>

#include <codegen/schema/fixed_point_form.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>

namespace lispb::schema {

struct FixedPointTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::fixed_point>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    IndexedColumn<codegen::FixedPointForm, Index> form;
    IndexedColumn<std::uint8_t, Index> signedness;
    IndexedColumn<std::uint32_t, Index> total_bits;
    IndexedColumn<std::uint32_t, Index> fractional_bits;
    IndexedColumn<codegen::FixedPointRounding, Index> rounding;
    IndexedColumn<ioj::MinMax<double>, Index> value_range;
};

} // namespace lispb::schema
