#include <lispb/schema/static_table_types.h>

#include <cassert>

namespace lispb::schema {

auto StaticTableTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(source.size())};
    source.push_back({});
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::static_table) << 24) |
                            row};
}
auto StaticTableTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::static_table);
    auto const row{id.index()};
    assert(row < source.size());
    return row;
}

} // namespace lispb::schema
