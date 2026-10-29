#include <codegen/schema/optional_sentinel_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto OptionalSentinelSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto OptionalSentinelSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
    name.push_back({});
    source.push_back({});
    sentinel.push_back({});
    return DeclarationHandle{
        (static_cast<std::uint32_t>(DeclarationKind::optional_sentinel) << 24) | (index + 1)};
}
auto OptionalSentinelSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return detail::clone_schema_row(*this, id, cpp_projection, name, source, sentinel);
}
void OptionalSentinelSchemas::clear() {
    detail::clear_schema_columns(cpp_projection, name, source, sentinel);
}

}
