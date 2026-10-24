#include <codegen/schema/record_member_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto RecordMemberSchemas::index(RecordMemberId id) const -> std::uint32_t {
    assert(id.value != 0);
    auto const row{id.value - 1};
    assert(row < name.size());
    return row;
}
auto RecordMemberSchemas::append() -> RecordMemberId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    settings_exclude.push_back(false);
    settings_name.push_back({});
    type.push_back({});
    count.push_back({});
    relationship.push_back({});
    initializer.push_back({});
    return RecordMemberId{index + 1};
}
auto RecordMemberSchemas::clone(RecordMemberId id) -> RecordMemberId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    settings_exclude[destination] = settings_exclude[source_index];
    settings_name[destination] = settings_name[source_index];
    type[destination] = type[source_index];
    count[destination] = count[source_index];
    relationship[destination] = relationship[source_index];
    initializer[destination] = initializer[source_index];
    return result;
}
void RecordMemberSchemas::clear() {
    name.clear();
    settings_exclude.clear();
    settings_name.clear();
    type.clear();
    count.clear();
    relationship.clear();
    initializer.clear();
}

}
