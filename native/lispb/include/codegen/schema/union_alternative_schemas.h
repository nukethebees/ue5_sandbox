#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct UnionAlternativeSchemas {
    [[nodiscard]] auto append() -> UnionAlternativeId;
    [[nodiscard]] auto clone(UnionAlternativeId id) -> UnionAlternativeId;
    [[nodiscard]] auto index(UnionAlternativeId id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<TypeRefId> type;
    std::vector<std::optional<std::uint64_t>> count;
};

}
