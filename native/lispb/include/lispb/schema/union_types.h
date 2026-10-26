#pragma once

#include <codegen/schema/layout_kind.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/semantic_range.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lispb::schema {

struct UnionTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::union_type>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    [[nodiscard]] auto alternatives_of(TypeDefinitionId id) const
        -> std::span<ChildOccurrenceId const>;
    void set_alternatives(TypeDefinitionId id, std::span<ChildOccurrenceId const> values);

    IndexedColumn<codegen::LayoutKind, Index> layout;
    IndexedColumn<std::optional<std::uint32_t>, Index> bits;
    IndexedColumn<ResolvedOccurrenceId, Index> storage_type;
    IndexedColumn<SemanticRange, Index> alternatives;
    std::vector<ChildOccurrenceId> alternatives_values;
};

} // namespace lispb::schema
