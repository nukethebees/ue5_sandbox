#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <lispb/indexed_column.h>

#include <optional>

namespace lispb::codegen {

struct SemanticRelationSchemas {
    using Indexing = OneBasedIndex<SemanticRelationId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> SemanticRelationId;
    [[nodiscard]] auto clone(SemanticRelationId id) -> SemanticRelationId;
    [[nodiscard]] auto index(SemanticRelationId id) const -> std::uint32_t;
    void clear();

    Column<SemanticRelationKind> kind;
    Column<TypeRefId> target;
    Column<std::optional<SemanticRelationUnit>> unit;
};

}
