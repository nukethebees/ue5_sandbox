#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct StaticTableSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
    std::vector<SchemaRange<StaticTableRowId>> rows;
    std::vector<SchemaRange<StaticTableColumnId>> columns;
    std::vector<SchemaRange<StaticTableGroupId>> groups;
    std::vector<StringId> export_specifier;
};

}
