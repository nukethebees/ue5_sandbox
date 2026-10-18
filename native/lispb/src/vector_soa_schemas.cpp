#include <codegen/schema/vector_soa_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto VectorSoaSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    assert(id.kind() == DeclarationKind::vector_soa);
    auto const row{id.index()};
    assert(row < name.size());
    return row;
}
auto VectorSoaSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    value_type.push_back({});
    components.push_back({});
    equivalent_members.push_back({});
    equivalent_constructor.push_back({});
    equivalent_type.push_back({});
    export_specifier.push_back({});
    fixed.push_back({});
    equivalent_accessors.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::vector_soa) << 24) |
                             (index + 1)};
}
auto VectorSoaSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    value_type[destination] = value_type[source_index];
    components[destination] = components[source_index];
    equivalent_members[destination] = equivalent_members[source_index];
    equivalent_constructor[destination] = equivalent_constructor[source_index];
    equivalent_type[destination] = equivalent_type[source_index];
    export_specifier[destination] = export_specifier[source_index];
    fixed[destination] = fixed[source_index];
    equivalent_accessors[destination] = equivalent_accessors[source_index];
    return result;
}
void VectorSoaSchemas::clear() {
    name.clear();
    value_type.clear();
    components.clear();
    equivalent_members.clear();
    equivalent_constructor.clear();
    equivalent_type.clear();
    export_specifier.clear();
    fixed.clear();
    equivalent_accessors.clear();
}

}
