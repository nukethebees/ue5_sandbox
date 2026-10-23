#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct RecordMemberSchemas {
    [[nodiscard]] auto append() -> RecordMemberId;
    [[nodiscard]] auto clone(RecordMemberId id) -> RecordMemberId;
    [[nodiscard]] auto index(RecordMemberId id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
    std::vector<TypeRefId> type;
    std::vector<std::optional<std::uint64_t>> count;
    std::vector<std::optional<SemanticRelationId>> relationship;
    std::vector<std::optional<StringId>> initializer;
};

}
