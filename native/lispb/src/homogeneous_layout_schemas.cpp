#include <codegen/schema/homogeneous_layout_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto HomogeneousLayoutSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    assert(id.kind() == DeclarationKind::homogeneous_layout);
    auto const row{id.index()};
    assert(row < name.size());
    return row;
}
auto HomogeneousLayoutSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    components.push_back({});
    input_members.push_back({});
    value_types.push_back({});
    export_specifier.push_back({});
    return DeclarationHandle{
        (static_cast<std::uint32_t>(DeclarationKind::homogeneous_layout) << 24) | (index + 1)};
}
auto HomogeneousLayoutSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    components[destination] = components[source_index];
    input_members[destination] = input_members[source_index];
    value_types[destination] = value_types[source_index];
    export_specifier[destination] = export_specifier[source_index];
    return result;
}
void HomogeneousLayoutSchemas::clear() {
    name.clear();
    components.clear();
    input_members.clear();
    value_types.clear();
    export_specifier.clear();
}

}
