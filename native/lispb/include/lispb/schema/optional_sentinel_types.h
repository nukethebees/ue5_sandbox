#pragma once

#include <codegen/schema/packed_integer_value.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>

namespace lispb::schema {

struct OptionalSentinelTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::optional_sentinel>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    IndexedColumn<ResolvedOccurrenceId, Index> source;
    IndexedColumn<StringId, Index> sentinel_name;
    IndexedColumn<codegen::PackedIntegerValue, Index> sentinel_value;
    IndexedColumn<std::uint32_t, Index> bit_width;
};

} // namespace lispb::schema
