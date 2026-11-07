#include <codegen/schema/fixed_point_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {
auto FixedPointSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}

auto FixedPointSchemas::append(StringId const name_value,
                               FixedPointFormat const format_value,
                               std::optional<CppProjection> const projection,
                               FixedPointRounding const rounding_value) -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back(projection);
    name.push_back(name_value);
    format.push_back(format_value);
    rounding.push_back(rounding_value);
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::fixed_point) << 24) |
                             (index + 1)};
}
auto FixedPointSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return append(name[id], format[id], cpp_projection[id], rounding[id]);
}
void FixedPointSchemas::clear() {
    detail::clear_schema_columns(cpp_projection, name, format, rounding);
}
}
