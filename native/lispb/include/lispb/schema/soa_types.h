#pragma once

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
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    [[nodiscard]] auto columns_of(TypeDefinitionId id) const -> std::span<SoaColumnId const>;
    void set_columns(TypeDefinitionId id, std::span<SoaColumnId const> values);

    std::vector<SemanticRange> columns;
    std::vector<SoaColumnId> columns_values;
    std::vector<ResolvedOccurrenceId> equivalent_type;
    std::vector<std::uint8_t> layout_only;
    std::vector<std::optional<StringId>> equivalent_constructor;
};

} // namespace lispb::schema
