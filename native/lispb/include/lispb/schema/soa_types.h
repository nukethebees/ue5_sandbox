#pragma once

#include <codegen/schema/soa_backend.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/semantic_range.h>
#include <lispb/schema/soa_source_kind.h>
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
    [[nodiscard]] auto vector_components_of(TypeDefinitionId id) const
        -> std::span<IdentifierId const>;
    void set_vector_components(TypeDefinitionId id, std::span<IdentifierId const> values);

    std::vector<codegen::SoaBackend> backend;
    std::vector<SoaSourceKind> source_kind;
    std::vector<SemanticRange> columns;
    std::vector<SoaColumnId> columns_values;
    std::vector<ResolvedOccurrenceId> equivalent_type;
    std::vector<std::optional<StringId>> related_storage_name;
    std::vector<SemanticRange> vector_components;
    std::vector<IdentifierId> vector_components_values;
    std::vector<std::uint8_t> layout_only;
    std::vector<std::optional<StringId>> equivalent_constructor;
};

} // namespace lispb::schema
