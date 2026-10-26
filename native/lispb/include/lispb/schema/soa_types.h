#pragma once

#include <lispb/indexed_column.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/semantic_range.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lispb::schema {

struct SoaTypes {
    using Index = TaggedIndex<TypeDefinitionId, TypeDefinitionKind::soa>;

    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    [[nodiscard]] auto columns_of(TypeDefinitionId id) const -> std::span<SoaColumnId const>;
    void set_columns(TypeDefinitionId id, std::span<SoaColumnId const> values);

    IndexedColumn<SemanticRange, Index> columns;
    std::vector<SoaColumnId> columns_values;
    IndexedColumn<ResolvedOccurrenceId, Index> equivalent_type;
    IndexedColumn<std::uint8_t, Index> layout_only;
    IndexedColumn<std::optional<StringId>, Index> equivalent_constructor;
};

} // namespace lispb::schema
