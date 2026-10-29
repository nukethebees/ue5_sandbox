#include <codegen/schema/semantic_relation_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto SemanticRelationSchemas::index(SemanticRelationId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < kind.size());
    return row;
}
auto SemanticRelationSchemas::append() -> SemanticRelationId {
    auto const index{static_cast<std::uint32_t>(kind.size())};
    kind.push_back(SemanticRelationKind::references);
    target.push_back({});
    unit.push_back({});
    return SemanticRelationId{index + 1};
}
auto SemanticRelationSchemas::clone(SemanticRelationId id) -> SemanticRelationId {
    return detail::clone_schema_row(*this, id, kind, target, unit);
}
void SemanticRelationSchemas::clear() {
    detail::clear_schema_columns(kind, target, unit);
}

}
