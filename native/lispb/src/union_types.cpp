#include <lispb/schema/union_types.h>

#include <cassert>

namespace lispb::schema {

auto UnionTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(alternatives.size())};
    alternatives.push_back({});
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::union_type) << 24) |
                            row};
}
auto UnionTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::union_type);
    auto const row{id.index()};
    assert(row < alternatives.size());
    return row;
}
auto UnionTypes::alternatives_of(TypeDefinitionId const id) const
    -> std::span<UnionAlternativeId const> {
    auto const range{alternatives[index(id)]};
    return std::span{alternatives_values}.subspan(range.begin, range.count);
}
void UnionTypes::set_alternatives(TypeDefinitionId const id,
                                  std::span<UnionAlternativeId const> const values) {
    alternatives[index(id)] = {static_cast<std::uint32_t>(alternatives_values.size()),
                               static_cast<std::uint32_t>(values.size())};
    alternatives_values.insert(alternatives_values.end(), values.begin(), values.end());
}

} // namespace lispb::schema
