#include <codegen/schema/optional_presence_bit_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto OptionalPresenceBitSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto OptionalPresenceBitSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
    name.push_back({});
    source.push_back({});
    return DeclarationHandle{
        (static_cast<std::uint32_t>(DeclarationKind::optional_presence_bit) << 24) | (index + 1)};
}
auto OptionalPresenceBitSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return detail::clone_schema_row(*this, id, cpp_projection, name, source);
}
void OptionalPresenceBitSchemas::clear() {
    detail::clear_schema_columns(cpp_projection, name, source);
}

}
