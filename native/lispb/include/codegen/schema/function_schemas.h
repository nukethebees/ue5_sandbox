#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct FunctionSchemas {
    [[nodiscard]] auto append() -> FunctionSchemaId;
    [[nodiscard]] auto clone(FunctionSchemaId id) -> FunctionSchemaId;
    [[nodiscard]] auto index(FunctionSchemaId id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<TypeRefId> return_type;
    std::vector<SchemaRange<ParameterId>> parameters;
    std::vector<SchemaRange<StringId>> body_lines;
    std::vector<SchemaRange<IdentifierId>> dependencies;
    std::vector<std::optional<TypeRefId>> trailing_return_type;
    std::vector<std::uint8_t> is_const;
    std::vector<std::uint8_t> is_noexcept;
    std::vector<std::uint8_t> is_static;
    std::vector<std::uint8_t> is_inline;
    std::vector<std::uint8_t> definition_in_source;
    std::vector<std::optional<StringId>> template_parameters;
    std::vector<std::optional<StringId>> requires_clause;
    std::vector<std::uint8_t> is_constexpr;
    std::vector<std::uint8_t> is_nodiscard;
};

}
