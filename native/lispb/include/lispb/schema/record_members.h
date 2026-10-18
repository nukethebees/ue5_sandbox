#pragma once

#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {

struct RecordMembers {
    [[nodiscard]] auto append() -> RecordMemberId;
    [[nodiscard]] auto index(RecordMemberId id) const -> std::uint32_t;

    std::vector<IdentifierId> name;
    std::vector<ResolvedOccurrenceId> semantic_type;
    std::vector<std::optional<std::uint64_t>> count;
    std::vector<RelationshipId> relationship;
    std::vector<std::optional<StringId>> initializer;
};

} // namespace lispb::schema
