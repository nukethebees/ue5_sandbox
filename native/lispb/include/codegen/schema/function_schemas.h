#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct FunctionSchemas {
    using Indexing = OneBasedIndex<FunctionSchemaId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> FunctionSchemaId;
    [[nodiscard]] auto clone(FunctionSchemaId id) -> FunctionSchemaId;
    [[nodiscard]] auto index(FunctionSchemaId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<TypeRefId> return_type;
    Column<SchemaRange<ParameterId>> parameters;
    Column<SchemaRange<StringId>> body_lines;
    Column<std::optional<SchemaRange<StringId>>> append_columns;
    Column<SchemaRange<StringId>> dependencies;
    Column<std::optional<TypeRefId>> trailing_return_type;
    Column<std::uint8_t> is_const;
    Column<std::uint8_t> is_noexcept;
    Column<std::uint8_t> is_static;
    Column<std::uint8_t> is_inline;
    Column<std::uint8_t> definition_in_source;
    Column<std::optional<StringId>> template_parameters;
    Column<std::optional<StringId>> requires_clause;
    Column<std::uint8_t> is_constexpr;
    Column<std::uint8_t> is_nodiscard;
};

}
