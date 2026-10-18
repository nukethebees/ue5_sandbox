#include <lispb/schema/relationships.h>

#include <cassert>

namespace lispb::schema {

auto Relationships::append() -> RelationshipId {
    auto const id{RelationshipId{static_cast<std::uint32_t>(kind.size() + 1)}};
    kind.push_back({});
    target.push_back({});
    unit.push_back({});
    return id;
}
auto Relationships::index(RelationshipId const id) const -> std::uint32_t {
    assert(lispb::schema::is_valid(id) && id.value <= kind.size());
    return id.value - 1;
}

} // namespace lispb::schema
