#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/soa_member_kind.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <limits>
#include <optional>

namespace lispb::schema {

struct SoaColumns {
    using Index = ZeroBasedIndex<SoaColumnId, std::numeric_limits<std::uint32_t>::max()>;

    [[nodiscard]] auto append() -> SoaColumnId;
    [[nodiscard]] auto index(SoaColumnId id) const -> std::uint32_t;

    IndexedColumn<std::optional<codegen::SoaMemberId>, Index> source;
    IndexedColumn<StringId, Index> name;
    IndexedColumn<ResolvedOccurrenceId, Index> semantic_type;
    IndexedColumn<codegen::SoaMemberKind, Index> kind;
    IndexedColumn<RelationshipId, Index> relationship;
};

} // namespace lispb::schema
