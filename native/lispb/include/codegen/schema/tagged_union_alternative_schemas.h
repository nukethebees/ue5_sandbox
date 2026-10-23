#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct TaggedUnionAlternativeSchemas {
    [[nodiscard]] auto append() -> TaggedUnionAlternativeId;
    [[nodiscard]] auto clone(TaggedUnionAlternativeId id) -> TaggedUnionAlternativeId;
    [[nodiscard]] auto index(TaggedUnionAlternativeId id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
    std::vector<TypeRefId> type;
    std::vector<std::optional<std::uint64_t>> count;
    std::vector<StringId> tag;
};

}
