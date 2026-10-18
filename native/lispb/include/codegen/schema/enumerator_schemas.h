#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct EnumeratorSchemas {
    [[nodiscard]] auto append() -> EnumeratorId;
    [[nodiscard]] auto clone(EnumeratorId id) -> EnumeratorId;
    [[nodiscard]] auto index(EnumeratorId id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<std::optional<StringId>> initializer;
    std::vector<std::optional<StringId>> display_name;
    std::vector<std::uint8_t> hidden;
    std::vector<std::optional<StringId>> serialized_name;
    std::vector<std::uint8_t> sentinel;
};

}
