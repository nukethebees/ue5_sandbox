#pragma once

#include <codegen/ast/access.h>
#include <codegen/ast/record_kind.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>

namespace lispb::codegen {

struct FacadeSchemas {
    using Indexing = TaggedIndex<DeclarationHandle, DeclarationKind::facade>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<TypeRefId> target_type;
    Column<StringId> target_member_name;
    Column<SchemaRange<FacadeMethodId>> methods;
    Column<SchemaRange<StringId>> validation_lines;
    Column<SchemaRange<StringId>> validation_dependencies;
    Column<StringId> export_specifier;
    Column<Access> bind_access;
    Column<Access> method_access;
    Column<SchemaRange<StringId>> friends;
    Column<RecordKind> friend_kind;
    Column<std::uint8_t> definitions_in_source;
    Column<std::uint8_t> reference_target;
};

}
