#include <lispb/schema/external_types.h>

#include <cassert>

namespace lispb::schema {

auto ExternalTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(cpp_type.size())};
    cpp_type.push_back({});
    registered_names.push_back({});
    integer_semantics.push_back({});
    floating_semantics.push_back({});
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::external) << 24) | row};
}
auto ExternalTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::external);
    auto const row{id.index()};
    assert(row < cpp_type.size());
    return row;
}
auto ExternalTypes::registered_names_of(TypeDefinitionId const id) const
    -> std::span<StringId const> {
    auto const range{registered_names[index(id)]};
    return std::span{registered_names_values}.subspan(range.begin, range.count);
}
void ExternalTypes::set_registered_names(TypeDefinitionId const id,
                                         std::span<StringId const> const values) {
    registered_names[index(id)] = {static_cast<std::uint32_t>(registered_names_values.size()),
                                   static_cast<std::uint32_t>(values.size())};
    registered_names_values.insert(registered_names_values.end(), values.begin(), values.end());
}

} // namespace lispb::schema
