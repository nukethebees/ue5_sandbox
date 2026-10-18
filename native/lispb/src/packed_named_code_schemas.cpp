#include <codegen/schema/packed_named_code_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto PackedNamedCodeSchemas::index(PackedNamedCodeId id) const -> std::uint32_t {
    assert(id.value != 0);
    auto const row{id.value - 1};
    assert(row < name.size());
    return row;
}
auto PackedNamedCodeSchemas::append() -> PackedNamedCodeId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    value.push_back({});
    sentinel.push_back({});
    return PackedNamedCodeId{index + 1};
}
auto PackedNamedCodeSchemas::clone(PackedNamedCodeId id) -> PackedNamedCodeId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    value[destination] = value[source_index];
    sentinel[destination] = sentinel[source_index];
    return result;
}
void PackedNamedCodeSchemas::clear() {
    name.clear();
    value.clear();
    sentinel.clear();
}

}
