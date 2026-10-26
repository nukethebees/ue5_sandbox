#include <codegen/schema/child_occurrences.h>

#include <cassert>

namespace lispb::codegen {

auto ChildOccurrences::index(ChildOccurrenceId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto ChildOccurrences::append() -> ChildOccurrenceId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    kind.push_back(ChildOccurrenceKind::value);
    name.push_back({});
    type.push_back({});
    count.push_back({});
    bits.push_back({});
    case_label.push_back({});
    tag.push_back({});
    relationship.push_back({});
    initializer.push_back({});
    s7.push_back({});
    default_value.push_back({});
    return ChildOccurrenceId{index + 1};
}
auto ChildOccurrences::clone(ChildOccurrenceId id) -> ChildOccurrenceId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    kind[destination] = kind[source_index];
    name[destination] = name[source_index];
    type[destination] = type[source_index];
    count[destination] = count[source_index];
    bits[destination] = bits[source_index];
    case_label[destination] = case_label[source_index];
    tag[destination] = tag[source_index];
    relationship[destination] = relationship[source_index];
    initializer[destination] = initializer[source_index];
    s7[destination] = s7[source_index];
    default_value[destination] = default_value[source_index];
    return result;
}
void ChildOccurrences::clear() {
    kind.clear();
    name.clear();
    type.clear();
    count.clear();
    bits.clear();
    case_label.clear();
    tag.clear();
    relationship.clear();
    initializer.clear();
    s7.clear();
    default_value.clear();
}

}
