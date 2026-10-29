#include <codegen/schema/parameter_schemas.h>

#include "schema_columns.h"

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
    return detail::clone_schema_row(*this, id, type, name, default_value);
}
void ParameterSchemas::clear() {
    detail::clear_schema_columns(type, name, default_value);
}

}
