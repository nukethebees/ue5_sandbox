#include <lispb/schema/child_occurrences.h>

#include <cassert>

namespace lispb::schema {

auto ChildOccurrences::append() -> ChildOccurrenceId {
    auto const id{ChildOccurrenceId{static_cast<std::uint32_t>(name.size() + 1)}};
    source.emplace_back();
    name.emplace_back();
    semantic_type.emplace_back();
    count.emplace_back();
    relationship.emplace_back();
    initializer.emplace_back();
    bits.emplace_back();
    case_label.emplace_back();
    tag.emplace_back();
    default_value.emplace_back();
    reserved.emplace_back();
    return id;
}
auto ChildOccurrences::index(ChildOccurrenceId const id) const -> std::uint32_t {
    assert(id.value && id.value <= name.size());
    return id.value - 1;
}

}
