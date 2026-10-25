#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct SoaSchemas {
    using Indexing = TaggedIndex<DeclarationHandle, DeclarationKind::soa>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<SchemaRange<SoaMemberId>> members;
    Column<StringId> export_specifier;
    Column<SchemaRange<FunctionSchemaId>> functions;
    Column<SchemaRange<FunctionSchemaId>> mutable_view_functions;
    Column<SchemaRange<StringId>> using_declarations;
    Column<std::optional<TypeRefId>> equivalent_type;
    Column<std::optional<StringId>> equivalent_constructor;
    Column<std::uint8_t> layout_only;
    Column<std::optional<StringId>> field_mask_name;
    Column<std::optional<StringId>> field_enum_name;
    Column<SchemaRange<FunctionSchemaId>> const_view_functions;
};

}
