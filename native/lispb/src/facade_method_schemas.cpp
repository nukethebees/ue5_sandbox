#include <codegen/schema/facade_method_schemas.h>

#include "schema_columns.h"

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
    return detail::clone_schema_row(
        *this, id, name, return_type, parameters, is_const, is_noexcept, target_name);
}
void FacadeMethodSchemas::clear() {
    detail::clear_schema_columns(name, return_type, parameters, is_const, is_noexcept, target_name);
}

}
