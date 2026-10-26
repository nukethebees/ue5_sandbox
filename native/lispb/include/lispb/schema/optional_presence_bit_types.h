#pragma once

#include <lispb/indexed_column.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>

namespace lispb::schema {

struct OptionalPresenceBitTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::optional_presence_bit>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    IndexedColumn<ResolvedOccurrenceId, Index> source;
    IndexedColumn<std::uint32_t, Index> payload_bits;
    IndexedColumn<std::uint32_t, Index> encoded_bits;
};

} // namespace lispb::schema
