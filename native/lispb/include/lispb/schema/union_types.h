#pragma once

#include <lispb/schema/indices.h>
#include <lispb/schema/semantic_range.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <span>
#include <vector>

namespace lispb::schema {

struct UnionTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    [[nodiscard]] auto alternatives_of(TypeDefinitionId id) const
        -> std::span<UnionAlternativeId const>;
    void set_alternatives(TypeDefinitionId id, std::span<UnionAlternativeId const> values);

    std::vector<SemanticRange> alternatives;
    std::vector<UnionAlternativeId> alternatives_values;
};

} // namespace lispb::schema
