#include <codegen/schema/packed_named_code_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto PackedNamedCodeSchemas::index(PackedNamedCodeId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto PackedNamedCodeSchemas::append() -> PackedNamedCodeId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    value.push_back({});
    sentinel.push_back({});
    return PackedNamedCodeId{index + 1};
}
auto PackedNamedCodeSchemas::clone(PackedNamedCodeId id) -> PackedNamedCodeId {
    return detail::clone_schema_row(*this, id, name, value, sentinel);
}
void PackedNamedCodeSchemas::clear() {
    detail::clear_schema_columns(name, value, sentinel);
}

}
