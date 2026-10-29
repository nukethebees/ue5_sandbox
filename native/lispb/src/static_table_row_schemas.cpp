#include <codegen/schema/static_table_row_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto StaticTableRowSchemas::index(StaticTableRowId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto StaticTableRowSchemas::append() -> StaticTableRowId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    return StaticTableRowId{index + 1};
}
auto StaticTableRowSchemas::clone(StaticTableRowId id) -> StaticTableRowId {
    return detail::clone_schema_row(*this, id, name);
}
void StaticTableRowSchemas::clear() {
    detail::clear_schema_columns(name);
}

}
