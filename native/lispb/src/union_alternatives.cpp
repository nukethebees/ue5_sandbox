#include <lispb/schema/union_alternatives.h>

#include <cassert>

namespace lispb::schema {

auto UnionAlternatives::append() -> UnionAlternativeId {
    auto const row{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    semantic_type.push_back({});
    count.push_back({});
    return UnionAlternativeId{row};
}
auto UnionAlternatives::index(UnionAlternativeId const id) const -> std::uint32_t {
    auto const row{id.value};
    assert(row < name.size());
    return row;
}

} // namespace lispb::schema
