#include <codegen/schema/union_alternative_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto UnionAlternativeSchemas::index(UnionAlternativeId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto UnionAlternativeSchemas::append() -> UnionAlternativeId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    type.push_back({});
    count.push_back({});
    return UnionAlternativeId{index + 1};
}
auto UnionAlternativeSchemas::clone(UnionAlternativeId id) -> UnionAlternativeId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    type[destination] = type[source_index];
    count[destination] = count[source_index];
    return result;
}
void UnionAlternativeSchemas::clear() {
    name.clear();
    type.clear();
    count.clear();
}

}
