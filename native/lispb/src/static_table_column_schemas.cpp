#include <codegen/schema/static_table_column_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto StaticTableColumnSchemas::index(StaticTableColumnId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto StaticTableColumnSchemas::append() -> StaticTableColumnId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    type.push_back({});
    return StaticTableColumnId{index + 1};
}
auto StaticTableColumnSchemas::clone(StaticTableColumnId id) -> StaticTableColumnId {
    return detail::clone_schema_row(*this, id, name, type);
}
void StaticTableColumnSchemas::clear() {
    detail::clear_schema_columns(name, type);
}

}
