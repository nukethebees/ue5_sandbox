#include <codegen/schema/soa_schemas.h>

#include "schema_columns.h"

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
    return detail::clone_schema_row(*this,
                                    id,
                                    name,
                                    members,
                                    export_specifier,
                                    functions,
                                    mutable_view_functions,
                                    using_declarations,
                                    equivalent_type,
                                    equivalent_constructor,
                                    layout_only,
                                    field_mask_name,
                                    field_enum_name,
                                    const_view_functions);
}
void SoaSchemas::clear() {
    detail::clear_schema_columns(name,
                                 members,
                                 export_specifier,
                                 functions,
                                 mutable_view_functions,
                                 using_declarations,
                                 equivalent_type,
                                 equivalent_constructor,
                                 layout_only,
                                 field_mask_name,
                                 field_enum_name,
                                 const_view_functions);
}

}
