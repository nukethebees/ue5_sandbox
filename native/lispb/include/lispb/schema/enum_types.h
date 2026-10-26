#pragma once

#include <lispb/indexed_column.h>
#include <lispb/indices.h>
#include <lispb/schema/enum_signedness.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>

namespace lispb::schema {

struct EnumTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::enumeration>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    IndexedColumn<ResolvedOccurrenceId, Index> underlying_type;
    IndexedColumn<EnumDomainId, Index> domain;
    IndexedColumn<std::optional<std::uint8_t>, Index> bit_width;
    IndexedColumn<EnumSignedness, Index> signedness;
    IndexedColumn<std::optional<StringId>, Index> count;
    IndexedColumn<std::uint8_t, Index> enum_array;
};

} // namespace lispb::schema
