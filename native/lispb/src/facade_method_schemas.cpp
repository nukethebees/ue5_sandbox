#include <codegen/schema/facade_method_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto FacadeMethodSchemas::index(FacadeMethodId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto FacadeMethodSchemas::append() -> FacadeMethodId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    return_type.push_back({});
    parameters.push_back({});
    is_const.push_back(false);
    is_noexcept.push_back(false);
    target_name.push_back({});
    return FacadeMethodId{index + 1};
}
auto FacadeMethodSchemas::clone(FacadeMethodId id) -> FacadeMethodId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    return_type[destination] = return_type[source_index];
    parameters[destination] = parameters[source_index];
    is_const[destination] = is_const[source_index];
    is_noexcept[destination] = is_noexcept[source_index];
    target_name[destination] = target_name[source_index];
    return result;
}
void FacadeMethodSchemas::clear() {
    name.clear();
    return_type.clear();
    parameters.clear();
    is_const.clear();
    is_noexcept.clear();
    target_name.clear();
}

}
