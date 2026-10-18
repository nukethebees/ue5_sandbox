#pragma once

#include <lispb/schema/indices.h>
#include <lispb/schema/semantic_range.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <span>
#include <vector>

namespace lispb::schema {

struct RecordTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    [[nodiscard]] auto members_of(TypeDefinitionId id) const -> std::span<RecordMemberId const>;
    void set_members(TypeDefinitionId id, std::span<RecordMemberId const> values);

    std::vector<SemanticRange> members;
    std::vector<RecordMemberId> members_values;
};

} // namespace lispb::schema
