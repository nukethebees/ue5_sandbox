#include <lispb/schema/tagged_union_types.h>

#include <cassert>

namespace lispb::schema {

auto TaggedUnionTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(discriminant.size())};
    discriminant.push_back({});
    alternatives.push_back({});
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::tagged_union) << 24) |
                            row};
}
auto TaggedUnionTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::tagged_union);
    auto const row{id.index()};
    assert(row < discriminant.size());
    return row;
}
auto TaggedUnionTypes::alternatives_of(TypeDefinitionId const id) const
    -> std::span<TaggedUnionAlternativeId const> {
    auto const range{alternatives[index(id)]};
    return std::span{alternatives_values}.subspan(range.begin, range.count);
}
void TaggedUnionTypes::set_alternatives(TypeDefinitionId const id,
                                        std::span<TaggedUnionAlternativeId const> const values) {
    alternatives[index(id)] = {static_cast<std::uint32_t>(alternatives_values.size()),
                               static_cast<std::uint32_t>(values.size())};
    alternatives_values.insert(alternatives_values.end(), values.begin(), values.end());
}

} // namespace lispb::schema
