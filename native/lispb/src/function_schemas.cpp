#include <codegen/schema/function_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto FunctionSchemas::index(FunctionSchemaId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto FunctionSchemas::append() -> FunctionSchemaId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    return_type.push_back({});
    parameters.push_back({});
    body_lines.push_back({});
    append_columns.push_back({});
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
    return detail::clone_schema_row(*this,
                                    id,
                                    name,
                                    return_type,
                                    parameters,
                                    body_lines,
                                    append_columns,
                                    dependencies,
                                    trailing_return_type,
                                    is_const,
                                    is_noexcept,
                                    is_static,
                                    is_inline,
                                    definition_in_source,
                                    template_parameters,
                                    requires_clause,
                                    is_constexpr,
                                    is_nodiscard);
}
void FunctionSchemas::clear() {
    detail::clear_schema_columns(name,
                                 return_type,
                                 parameters,
                                 body_lines,
                                 append_columns,
                                 dependencies,
                                 trailing_return_type,
                                 is_const,
                                 is_noexcept,
                                 is_static,
                                 is_inline,
                                 definition_in_source,
                                 template_parameters,
                                 requires_clause,
                                 is_constexpr,
                                 is_nodiscard);
}

}
