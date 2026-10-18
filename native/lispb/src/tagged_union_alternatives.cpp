#include <lispb/schema/tagged_union_alternatives.h>

#include <cassert>

namespace lispb::schema {

auto TaggedUnionAlternatives::append() -> TaggedUnionAlternativeId {
    auto const row{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    semantic_type.push_back({});
    count.push_back({});
    tag.push_back({});
    return TaggedUnionAlternativeId{row};
}
auto TaggedUnionAlternatives::index(TaggedUnionAlternativeId const id) const -> std::uint32_t {
    auto const row{id.value};
    assert(row < name.size());
    return row;
}

} // namespace lispb::schema
