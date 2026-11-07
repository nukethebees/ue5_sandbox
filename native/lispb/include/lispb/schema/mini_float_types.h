#pragma once

#include <lispb/indexed_column.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>

namespace lispb::schema {

struct MiniFloatTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::mini_float>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    IndexedColumn<std::uint32_t, Index> sign_bits;
    IndexedColumn<std::uint32_t, Index> exponent_bits;
    IndexedColumn<std::uint32_t, Index> significand_bits;
    IndexedColumn<std::int32_t, Index> exponent_bias;
};

} // namespace lispb::schema
