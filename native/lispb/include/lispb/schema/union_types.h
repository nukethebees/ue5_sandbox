#pragma once

#include <codegen/schema/layout_kind.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/semantic_range.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lispb::schema {

struct UnionTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    [[nodiscard]] auto alternatives_of(TypeDefinitionId id) const
        -> std::span<ChildOccurrenceId const>;
    void set_alternatives(TypeDefinitionId id, std::span<ChildOccurrenceId const> values);

    std::vector<codegen::LayoutKind> layout;
    std::vector<std::optional<std::uint32_t>> bits;
    std::vector<ResolvedOccurrenceId> storage_type;
    std::vector<SemanticRange> alternatives;
    std::vector<ChildOccurrenceId> alternatives_values;
};

} // namespace lispb::schema
