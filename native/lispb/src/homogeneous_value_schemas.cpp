#include <codegen/schema/homogeneous_value_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto HomogeneousValueSchemas::index(HomogeneousValueId id) const -> std::uint32_t {
    assert(id.value != 0);
    auto const row{id.value - 1};
    assert(row < type.size());
    return row;
}
auto HomogeneousValueSchemas::append() -> HomogeneousValueId {
    auto const index{static_cast<std::uint32_t>(type.size())};
    type.push_back({});
    suffix.push_back({});
    equivalent_type.push_back({});
    input_types.push_back({});
    return HomogeneousValueId{index + 1};
}
auto HomogeneousValueSchemas::clone(HomogeneousValueId id) -> HomogeneousValueId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    type[destination] = type[source_index];
    suffix[destination] = suffix[source_index];
    equivalent_type[destination] = equivalent_type[source_index];
    input_types[destination] = input_types[source_index];
    return result;
}
void HomogeneousValueSchemas::clear() {
    type.clear();
    suffix.clear();
    equivalent_type.clear();
    input_types.clear();
}

}
