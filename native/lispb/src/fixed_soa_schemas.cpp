#include <codegen/schema/fixed_soa_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto FixedSoaSchemas::index(FixedSoaId id) const -> std::uint32_t {
    assert(id.value != 0);
    auto const row{id.value - 1};
    assert(row < storage_name.size());
    return row;
}
auto FixedSoaSchemas::append() -> FixedSoaId {
    auto const index{static_cast<std::uint32_t>(storage_name.size())};
    storage_name.push_back({});
    containers.push_back({});
    return FixedSoaId{index + 1};
}
auto FixedSoaSchemas::clone(FixedSoaId id) -> FixedSoaId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    storage_name[destination] = storage_name[source_index];
    containers[destination] = containers[source_index];
    return result;
}
void FixedSoaSchemas::clear() {
    storage_name.clear();
    containers.clear();
}

}
