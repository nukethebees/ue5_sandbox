#include <codegen/schema/static_table_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto StaticTableSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    assert(id.kind() == DeclarationKind::static_table);
    auto const row{id.index()};
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
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    rows[destination] = rows[source_index];
    columns[destination] = columns[source_index];
    groups[destination] = groups[source_index];
    export_specifier[destination] = export_specifier[source_index];
    return result;
}
void StaticTableSchemas::clear() {
    name.clear();
    rows.clear();
    columns.clear();
    groups.clear();
    export_specifier.clear();
}

}
