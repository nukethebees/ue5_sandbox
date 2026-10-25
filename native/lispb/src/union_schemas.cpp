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
    name[destination] = name[source_index];
    alternatives[destination] = alternatives[source_index];
    export_specifier[destination] = export_specifier[source_index];
    return result;
}
void UnionSchemas::clear() {
    name.clear();
    alternatives.clear();
    export_specifier.clear();
}

}
