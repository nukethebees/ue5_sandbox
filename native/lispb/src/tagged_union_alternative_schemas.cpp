#include <codegen/schema/tagged_union_alternative_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto TaggedUnionAlternativeSchemas::index(TaggedUnionAlternativeId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto TaggedUnionAlternativeSchemas::append() -> TaggedUnionAlternativeId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    type.push_back({});
    count.push_back({});
    tag.push_back({});
    return TaggedUnionAlternativeId{index + 1};
}
auto TaggedUnionAlternativeSchemas::clone(TaggedUnionAlternativeId id) -> TaggedUnionAlternativeId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    type[destination] = type[source_index];
    count[destination] = count[source_index];
    tag[destination] = tag[source_index];
    return result;
}
void TaggedUnionAlternativeSchemas::clear() {
    name.clear();
    type.clear();
    count.clear();
    tag.clear();
}

}
