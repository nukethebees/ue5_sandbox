#include <lispb/schema/homogeneous_storage_types.h>

#include <cassert>

namespace lispb::schema {

auto HomogeneousStorageTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(components.size())};
    components.push_back({});
    value_type.push_back({});
    equivalent_type.push_back({});
    input_members.push_back({});
    input_types.push_back({});
    view_template_name.push_back({});
    return TypeDefinitionId{
        (static_cast<std::uint32_t>(TypeDefinitionKind::homogeneous_storage) << 24) | row};
}
auto HomogeneousStorageTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::homogeneous_storage);
    auto const row{id.index()};
    assert(row < components.size());
    return row;
}
auto HomogeneousStorageTypes::components_of(TypeDefinitionId const id) const
    -> std::span<IdentifierId const> {
    auto const range{components[index(id)]};
    return std::span{components_values}.subspan(range.begin, range.count);
}
void HomogeneousStorageTypes::set_components(TypeDefinitionId const id,
                                             std::span<IdentifierId const> const values) {
    components[index(id)] = {static_cast<std::uint32_t>(components_values.size()),
                             static_cast<std::uint32_t>(values.size())};
    components_values.insert(components_values.end(), values.begin(), values.end());
}
auto HomogeneousStorageTypes::input_members_of(TypeDefinitionId const id) const
    -> std::span<IdentifierId const> {
    auto const range{input_members[index(id)]};
    return std::span{input_members_values}.subspan(range.begin, range.count);
}
void HomogeneousStorageTypes::set_input_members(TypeDefinitionId const id,
                                                std::span<IdentifierId const> const values) {
    input_members[index(id)] = {static_cast<std::uint32_t>(input_members_values.size()),
                                static_cast<std::uint32_t>(values.size())};
    input_members_values.insert(input_members_values.end(), values.begin(), values.end());
}
auto HomogeneousStorageTypes::input_types_of(TypeDefinitionId const id) const
    -> std::span<ResolvedOccurrenceId const> {
    auto const range{input_types[index(id)]};
    return std::span{input_types_values}.subspan(range.begin, range.count);
}
void HomogeneousStorageTypes::set_input_types(TypeDefinitionId const id,
                                              std::span<ResolvedOccurrenceId const> const values) {
    input_types[index(id)] = {static_cast<std::uint32_t>(input_types_values.size()),
                              static_cast<std::uint32_t>(values.size())};
    input_types_values.insert(input_types_values.end(), values.begin(), values.end());
}

} // namespace lispb::schema
