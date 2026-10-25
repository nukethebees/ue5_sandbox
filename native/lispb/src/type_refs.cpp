#include <codegen/schema/type_refs.h>

#include <cassert>

namespace lispb::codegen {

auto TypeRefs::index(TypeRefId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto TypeRefs::append() -> TypeRefId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    suffix.push_back({});
    nested.push_back({});
    return TypeRefId{index + 1};
}
auto TypeRefs::clone(TypeRefId id) -> TypeRefId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    suffix[destination] = suffix[source_index];
    nested[destination] = nested[source_index];
    return result;
}
void TypeRefs::clear() {
    name.clear();
    suffix.clear();
    nested.clear();
}

}
