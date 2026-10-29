#include <codegen/schema/enumerator_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto EnumeratorSchemas::index(EnumeratorId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto EnumeratorSchemas::append() -> EnumeratorId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    initializer.push_back({});
    display_name.push_back({});
    hidden.push_back(false);
    serialized_name.push_back({});
    sentinel.push_back(false);
    return EnumeratorId{index + 1};
}
auto EnumeratorSchemas::clone(EnumeratorId id) -> EnumeratorId {
    return detail::clone_schema_row(
        *this, id, name, initializer, display_name, hidden, serialized_name, sentinel);
}
void EnumeratorSchemas::clear() {
    detail::clear_schema_columns(
        name, initializer, display_name, hidden, serialized_name, sentinel);
}

}
