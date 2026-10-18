#pragma once

#include <lispb/indices.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::schema {

struct TaggedUnionAlternatives {
    [[nodiscard]] auto append() -> TaggedUnionAlternativeId;
    [[nodiscard]] auto index(TaggedUnionAlternativeId id) const -> std::uint32_t;

    std::vector<IdentifierId> name;
    std::vector<ResolvedOccurrenceId> semantic_type;
    std::vector<std::optional<std::uint64_t>> count;
    std::vector<IdentifierId> tag;
};

} // namespace lispb::schema
