#include <codegen/schema/integer_varint_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto IntegerVarintSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto IntegerVarintSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
    name.push_back({});
    source.push_back({});
    encoding.push_back(IntegerVarintEncoding::unsigned_varint);
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::integer_varint) << 24) |
                             (index + 1)};
}
auto IntegerVarintSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    return detail::clone_schema_row(*this, id, cpp_projection, name, source, encoding);
}
void IntegerVarintSchemas::clear() {
    detail::clear_schema_columns(cpp_projection, name, source, encoding);
}

}
