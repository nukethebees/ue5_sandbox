#pragma once

#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <lispb/indexed_column.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::schema {

struct Relationships {
    using Index = OneBasedIndex<RelationshipId>;

    [[nodiscard]] auto append() -> RelationshipId;
    // IDs must name a row in this store; zero denotes absence at optional uses.
    [[nodiscard]] auto index(RelationshipId id) const -> std::uint32_t;

    IndexedColumn<codegen::SemanticRelationKind, Index> kind;
    IndexedColumn<ResolvedOccurrenceId, Index> target;
    IndexedColumn<std::optional<codegen::SemanticRelationUnit>, Index> unit;
};

} // namespace lispb::schema
