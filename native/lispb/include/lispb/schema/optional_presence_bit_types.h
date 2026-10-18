#pragma once

#include <lispb/schema/indices.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <vector>

namespace lispb::schema {

struct OptionalPresenceBitTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;

    std::vector<ResolvedOccurrenceId> source;
    std::vector<std::uint32_t> payload_bits;
    std::vector<std::uint32_t> encoded_bits;
};

} // namespace lispb::schema
