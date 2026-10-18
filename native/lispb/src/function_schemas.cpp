#include <codegen/schema/function_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto FunctionSchemas::index(FunctionSchemaId id) const -> std::uint32_t {
    assert(id.value != 0);
    auto const row{id.value - 1};
    assert(row < name.size());
    return row;
}
auto FunctionSchemas::append() -> FunctionSchemaId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    return_type.push_back({});
    parameters.push_back({});
    body_lines.push_back({});
    dependencies.push_back({});
    trailing_return_type.push_back({});
    is_const.push_back(false);
    is_noexcept.push_back(false);
    is_static.push_back(false);
    is_inline.push_back(false);
    definition_in_source.push_back(false);
    template_parameters.push_back({});
    requires_clause.push_back({});
    is_constexpr.push_back(false);
    is_nodiscard.push_back(false);
    return FunctionSchemaId{index + 1};
}
auto FunctionSchemas::clone(FunctionSchemaId id) -> FunctionSchemaId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    return_type[destination] = return_type[source_index];
    parameters[destination] = parameters[source_index];
    body_lines[destination] = body_lines[source_index];
    dependencies[destination] = dependencies[source_index];
    trailing_return_type[destination] = trailing_return_type[source_index];
    is_const[destination] = is_const[source_index];
    is_noexcept[destination] = is_noexcept[source_index];
    is_static[destination] = is_static[source_index];
    is_inline[destination] = is_inline[source_index];
    definition_in_source[destination] = definition_in_source[source_index];
    template_parameters[destination] = template_parameters[source_index];
    requires_clause[destination] = requires_clause[source_index];
    is_constexpr[destination] = is_constexpr[source_index];
    is_nodiscard[destination] = is_nodiscard[source_index];
    return result;
}
void FunctionSchemas::clear() {
    name.clear();
    return_type.clear();
    parameters.clear();
    body_lines.clear();
    dependencies.clear();
    trailing_return_type.clear();
    is_const.clear();
    is_noexcept.clear();
    is_static.clear();
    is_inline.clear();
    definition_in_source.clear();
    template_parameters.clear();
    requires_clause.clear();
    is_constexpr.clear();
    is_nodiscard.clear();
}

}
