#pragma once

#include <ioj/min_max.h>

#include <codegen/schema/fixed_point_form.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/numeric_formats.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>

namespace lispb::schema {

struct FixedPointTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::fixed_point>;

    [[nodiscard]] auto append(codegen::FixedPointFormat format,
                              codegen::FixedPointRounding rounding) -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    IndexedColumn<codegen::FixedPointFormat, Index> format;
    IndexedColumn<codegen::FixedPointRounding, Index> rounding;
};

}
