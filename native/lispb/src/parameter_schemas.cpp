#include <codegen/schema/parameter_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto ParameterSchemas::index(ParameterId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < type.size());
    return row;
}
auto ParameterSchemas::append(TypeRefId const type_value,
                              CppIdentifier<> const name_value,
                              std::optional<StringId> const default_expression,
                              bool const pack) -> ParameterId {
    auto const index{static_cast<std::uint32_t>(type.size())};
    type.push_back(type_value);
    name.push_back(name_value);
    default_value.push_back(default_expression);
    is_pack.push_back(pack);
    return ParameterId{index + 1};
}
auto ParameterSchemas::clone(ParameterId id) -> ParameterId {
    return append(type[id], name[id], default_value[id], is_pack[id]);
}
void ParameterSchemas::clear() {
    detail::clear_schema_columns(type, name, default_value, is_pack);
}

}
