#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/record_comparison.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct RecordSchemas {
    using Indexing = TaggedIndex<DeclarationHandle, DeclarationKind::record>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    Column<std::optional<StringId>> constraint_policy;
    Column<std::uint8_t> generate_visitor;
    Column<std::optional<StringId>> s7_tag;
    Column<std::uint8_t> s7_positional;
    Column<std::uint8_t> s7_constructor;
    Column<SchemaRange<StringId>> s7_reader_includes;
    Column<StringId> name;
    Column<SchemaRange<RecordMemberId>> members;
    Column<StringId> export_specifier;
    Column<RecordComparison> comparison;
    Column<std::uint8_t> comparison_noexcept;
    Column<SchemaRange<FunctionSchemaId>> functions;
};

}
