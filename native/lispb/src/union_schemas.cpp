#include <codegen/schema/union_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto UnionSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto UnionSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
    layout.push_back(LayoutKind::native);
    bits.push_back({});
    name.push_back({});
    alternatives.push_back({});
    export_specifier.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::union_type) << 24) |
                             (index + 1)};
}
auto UnionSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return detail::clone_schema_row(
        *this, id, cpp_projection, layout, bits, name, alternatives, export_specifier);
}
void UnionSchemas::clear() {
    detail::clear_schema_columns(
        cpp_projection, layout, bits, name, alternatives, export_specifier);
}

}
