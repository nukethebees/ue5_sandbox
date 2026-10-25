#include <codegen/schema/enumerator_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto EnumeratorSchemas::index(EnumeratorId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto EnumeratorSchemas::append() -> EnumeratorId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    initializer.push_back({});
    display_name.push_back({});
    hidden.push_back(false);
    serialized_name.push_back({});
    sentinel.push_back(false);
    return EnumeratorId{index + 1};
}
auto EnumeratorSchemas::clone(EnumeratorId id) -> EnumeratorId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    initializer[destination] = initializer[source_index];
    display_name[destination] = display_name[source_index];
    hidden[destination] = hidden[source_index];
    serialized_name[destination] = serialized_name[source_index];
    sentinel[destination] = sentinel[source_index];
    return result;
}
void EnumeratorSchemas::clear() {
    name.clear();
    initializer.clear();
    display_name.clear();
    hidden.clear();
    serialized_name.clear();
    sentinel.clear();
}

}
