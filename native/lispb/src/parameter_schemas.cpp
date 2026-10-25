#include <codegen/schema/parameter_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto ParameterSchemas::index(ParameterId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < type.size());
    return row;
}
auto ParameterSchemas::append() -> ParameterId {
    auto const index{static_cast<std::uint32_t>(type.size())};
    type.push_back({});
    name.push_back({});
    default_value.push_back({});
    return ParameterId{index + 1};
}
auto ParameterSchemas::clone(ParameterId id) -> ParameterId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    type[destination] = type[source_index];
    name[destination] = name[source_index];
    default_value[destination] = default_value[source_index];
    return result;
}
void ParameterSchemas::clear() {
    type.clear();
    name.clear();
    default_value.clear();
}

}
