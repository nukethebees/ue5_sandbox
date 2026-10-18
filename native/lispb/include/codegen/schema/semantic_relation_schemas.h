#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>

#include <optional>
#include <vector>

namespace lispb::codegen {

struct SemanticRelationSchemas {
    [[nodiscard]] auto append() -> SemanticRelationId;
    [[nodiscard]] auto clone(SemanticRelationId id) -> SemanticRelationId;
    [[nodiscard]] auto index(SemanticRelationId id) const -> std::uint32_t;
    void clear();

    std::vector<SemanticRelationKind> kind;
    std::vector<TypeRefId> target;
    std::vector<std::optional<SemanticRelationUnit>> unit;
};

}
