#include <codegen/schema/integer_scalar_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto IntegerScalarSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto IntegerScalarSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
    name.push_back({});
    signedness.push_back({});
    minimum_value.push_back({});
    maximum_value.push_back({});
    bit_width.push_back({});
    named_codes.push_back({});
    relationship.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::integer_scalar) << 24) |
                             (index + 1)};
}
auto IntegerScalarSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return detail::clone_schema_row(*this,
                                    id,
                                    cpp_projection,
                                    name,
                                    signedness,
                                    minimum_value,
                                    maximum_value,
                                    bit_width,
                                    named_codes,
                                    relationship);
}
void IntegerScalarSchemas::clear() {
    detail::clear_schema_columns(cpp_projection,
                                 name,
                                 signedness,
                                 minimum_value,
                                 maximum_value,
                                 bit_width,
                                 named_codes,
                                 relationship);
}

}
