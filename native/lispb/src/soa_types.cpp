#include <lispb/schema/soa_types.h>

#include <cassert>

namespace lispb::schema {

auto SoaTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(columns.size())};
    columns.push_back({});
    equivalent_type.push_back({});
    layout_only.push_back({});
    equivalent_constructor.push_back({});
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::soa) << 24) | row};
}
auto SoaTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::soa);
    auto const row{id.index()};
    assert(row < columns.size());
    return row;
}
auto SoaTypes::columns_of(TypeDefinitionId const id) const -> std::span<SoaColumnId const> {
    auto const range{columns[index(id)]};
    return std::span{columns_values}.subspan(range.begin, range.count);
}
void SoaTypes::set_columns(TypeDefinitionId const id, std::span<SoaColumnId const> const values) {
    columns[index(id)] = {static_cast<std::uint32_t>(columns_values.size()),
                          static_cast<std::uint32_t>(values.size())};
    columns_values.insert(columns_values.end(), values.begin(), values.end());
}
} // namespace lispb::schema
