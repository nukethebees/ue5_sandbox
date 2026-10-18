#include <codegen/schema/semantic_relation_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto SemanticRelationSchemas::index(SemanticRelationId id) const -> std::uint32_t {
    assert(id.value != 0);
    auto const row{id.value - 1};
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
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    kind[destination] = kind[source_index];
    target[destination] = target[source_index];
    unit[destination] = unit[source_index];
    return result;
}
void SemanticRelationSchemas::clear() {
    kind.clear();
    target.clear();
    unit.clear();
}

}
