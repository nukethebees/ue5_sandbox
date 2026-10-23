#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen {

struct TypeRefs {
    [[nodiscard]] auto append() -> TypeRefId;
    [[nodiscard]] auto clone(TypeRefId id) -> TypeRefId;
    [[nodiscard]] auto index(TypeRefId id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> name;
    std::vector<StringId> suffix;
    std::vector<std::optional<StringId>> nested;
};

}
