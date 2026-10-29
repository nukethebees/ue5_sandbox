#include <codegen/schema/static_table_group_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto StaticTableGroupSchemas::index(StaticTableGroupId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto StaticTableGroupSchemas::append() -> StaticTableGroupId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    type.push_back({});
    columns.push_back({});
    return StaticTableGroupId{index + 1};
}
auto StaticTableGroupSchemas::clone(StaticTableGroupId id) -> StaticTableGroupId {
    return detail::clone_schema_row(*this, id, name, type, columns);
}
void StaticTableGroupSchemas::clear() {
    detail::clear_schema_columns(name, type, columns);
}

}
