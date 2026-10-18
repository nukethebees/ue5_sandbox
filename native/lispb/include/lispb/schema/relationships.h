#pragma once

#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {

struct Relationships {
    [[nodiscard]] auto append() -> RelationshipId;
    // IDs must name a row in this store; zero denotes absence at optional uses.
    [[nodiscard]] auto index(RelationshipId id) const -> std::uint32_t;

    std::vector<codegen::SemanticRelationKind> kind;
    std::vector<ResolvedOccurrenceId> target;
    std::vector<std::optional<codegen::SemanticRelationUnit>> unit;
};

} // namespace lispb::schema
