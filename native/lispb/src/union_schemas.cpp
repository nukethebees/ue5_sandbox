#include <codegen/schema/union_schemas.h>

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
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    cpp_projection[destination] = cpp_projection[source_index];
    layout[destination] = layout[source_index];
    bits[destination] = bits[source_index];
    name[destination] = name[source_index];
    alternatives[destination] = alternatives[source_index];
    export_specifier[destination] = export_specifier[source_index];
    return result;
}
void UnionSchemas::clear() {
    cpp_projection.clear();
    layout.clear();
    bits.clear();
    name.clear();
    alternatives.clear();
    export_specifier.clear();
}

}
