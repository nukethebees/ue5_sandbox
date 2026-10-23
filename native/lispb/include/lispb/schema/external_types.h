#pragma once

#include <codegen/ast/indices.h>
#include <codegen/schema/floating_point_format.h>
#include <lispb/indices.h>
#include <lispb/schema/semantic_range.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lispb::schema {

struct ExternalTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    [[nodiscard]] auto registered_names_of(TypeDefinitionId id) const -> std::span<StringId const>;
    void set_registered_names(TypeDefinitionId id, std::span<StringId const> values);

    std::vector<codegen::CppTypeId> cpp_type;
    std::vector<SemanticRange> registered_names;
    std::vector<StringId> registered_names_values;
    std::vector<std::optional<TypeDefinitionId>> integer_semantics;
    std::vector<std::optional<codegen::FloatingPointFormat>> floating_semantics;
};

} // namespace lispb::schema
