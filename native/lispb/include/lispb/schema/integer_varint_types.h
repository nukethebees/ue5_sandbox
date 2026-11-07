#pragma once

#include <codegen/schema/integer_varint_encoding.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>

namespace lispb::schema {

struct IntegerVarintTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::integer_varint>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    IndexedColumn<ResolvedOccurrenceId, Index> source;
    IndexedColumn<codegen::IntegerVarintEncoding, Index> encoding;
};

} // namespace lispb::schema
