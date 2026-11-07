#pragma once

#include <codegen/schema/numeric_formats.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>

namespace lispb::schema {

struct MiniFloatTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::mini_float>;

    [[nodiscard]] auto append(codegen::MiniFloatFormat format) -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    IndexedColumn<codegen::MiniFloatFormat, Index> format;
};

}
