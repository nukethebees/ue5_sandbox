#include <lispb/schema/record_members.h>

#include <cassert>

namespace lispb::schema {

auto RecordMembers::append() -> RecordMemberId {
    auto const row{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    semantic_type.push_back({});
    count.push_back({});
    relationship.push_back({});
    initializer.push_back({});
    return RecordMemberId{row};
}
auto RecordMembers::index(RecordMemberId const id) const -> std::uint32_t {
    auto const row{id.value};
    assert(row < name.size());
    return row;
}

} // namespace lispb::schema
