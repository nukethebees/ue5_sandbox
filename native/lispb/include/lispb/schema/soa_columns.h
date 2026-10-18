#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/soa_member_kind.h>
#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {

struct SoaColumns {
    [[nodiscard]] auto append() -> SoaColumnId;
    [[nodiscard]] auto index(SoaColumnId id) const -> std::uint32_t;

    std::vector<std::optional<codegen::SoaMemberId>> source;
    std::vector<IdentifierId> name;
    std::vector<ResolvedOccurrenceId> semantic_type;
    std::vector<codegen::SoaMemberKind> kind;
    std::vector<std::optional<TypeId>> nested_type;
    std::vector<RelationshipId> relationship;
};

} // namespace lispb::schema
