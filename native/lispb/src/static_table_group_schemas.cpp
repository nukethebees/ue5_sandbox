#include <codegen/schema/static_table_group_schemas.h>

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
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    type[destination] = type[source_index];
    columns[destination] = columns[source_index];
    return result;
}
void StaticTableGroupSchemas::clear() {
    name.clear();
    type.clear();
    columns.clear();
}

}
