#include <codegen/schema/soa_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto SoaSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto SoaSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    members.push_back({});
    export_specifier.push_back({});
    functions.push_back({});
    mutable_view_functions.push_back({});
    using_declarations.push_back({});
    equivalent_type.push_back({});
    equivalent_constructor.push_back({});
    layout_only.push_back(false);
    field_mask_name.push_back({});
    field_enum_name.push_back({});
    const_view_functions.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::soa) << 24) |
                             (index + 1)};
}
auto SoaSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    members[destination] = members[source_index];
    export_specifier[destination] = export_specifier[source_index];
    functions[destination] = functions[source_index];
    mutable_view_functions[destination] = mutable_view_functions[source_index];
    using_declarations[destination] = using_declarations[source_index];
    equivalent_type[destination] = equivalent_type[source_index];
    equivalent_constructor[destination] = equivalent_constructor[source_index];
    layout_only[destination] = layout_only[source_index];
    field_mask_name[destination] = field_mask_name[source_index];
    field_enum_name[destination] = field_enum_name[source_index];
    const_view_functions[destination] = const_view_functions[source_index];
    return result;
}
void SoaSchemas::clear() {
    name.clear();
    members.clear();
    export_specifier.clear();
    functions.clear();
    mutable_view_functions.clear();
    using_declarations.clear();
    equivalent_type.clear();
    equivalent_constructor.clear();
    layout_only.clear();
    field_mask_name.clear();
    field_enum_name.clear();
    const_view_functions.clear();
}

}
