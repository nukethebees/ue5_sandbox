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

struct HomogeneousStorageTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    [[nodiscard]] auto components_of(TypeDefinitionId id) const -> std::span<IdentifierId const>;
    void set_components(TypeDefinitionId id, std::span<IdentifierId const> values);
    [[nodiscard]] auto input_members_of(TypeDefinitionId id) const -> std::span<IdentifierId const>;
    void set_input_members(TypeDefinitionId id, std::span<IdentifierId const> values);
    [[nodiscard]] auto input_types_of(TypeDefinitionId id) const
        -> std::span<ResolvedOccurrenceId const>;
    void set_input_types(TypeDefinitionId id, std::span<ResolvedOccurrenceId const> values);

    std::vector<SemanticRange> components;
    std::vector<IdentifierId> components_values;
    std::vector<ResolvedOccurrenceId> value_type;
    std::vector<ResolvedOccurrenceId> equivalent_type;
    std::vector<SemanticRange> input_members;
    std::vector<IdentifierId> input_members_values;
    std::vector<SemanticRange> input_types;
    std::vector<ResolvedOccurrenceId> input_types_values;
    std::vector<StringId> view_template_name;
};

} // namespace lispb::schema
