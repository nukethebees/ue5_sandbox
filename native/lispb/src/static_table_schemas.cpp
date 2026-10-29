#include <codegen/schema/static_table_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto StaticTableSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto StaticTableSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    rows.push_back({});
    columns.push_back({});
    groups.push_back({});
    export_specifier.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::static_table) << 24) |
                             (index + 1)};
}
auto StaticTableSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return detail::clone_schema_row(*this, id, name, rows, columns, groups, export_specifier);
}
void StaticTableSchemas::clear() {
    detail::clear_schema_columns(name, rows, columns, groups, export_specifier);
}

}
