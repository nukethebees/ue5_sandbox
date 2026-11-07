#include <codegen/schema/mini_float_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {
auto MiniFloatSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}

auto MiniFloatSchemas::append(StringId const name_value,
                              MiniFloatFormat const format_value,
                              std::optional<CppProjection> const projection) -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back(projection);
    name.push_back(name_value);
    format.push_back(format_value);
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::mini_float) << 24) |
                             (index + 1)};
}
auto MiniFloatSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return append(name[id], format[id], cpp_projection[id]);
}
void MiniFloatSchemas::clear() {
    detail::clear_schema_columns(cpp_projection, name, format);
}
}
