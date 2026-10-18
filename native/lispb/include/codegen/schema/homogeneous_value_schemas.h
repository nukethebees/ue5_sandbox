#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen {

struct HomogeneousValueSchemas {
    [[nodiscard]] auto append() -> HomogeneousValueId;
    [[nodiscard]] auto clone(HomogeneousValueId id) -> HomogeneousValueId;
    [[nodiscard]] auto index(HomogeneousValueId id) const -> std::uint32_t;
    void clear();

    std::vector<TypeRefId> type;
    std::vector<StringId> suffix;
    std::vector<std::optional<TypeRefId>> equivalent_type;
    std::vector<SchemaRange<TypeRefId>> input_types;
};

}
