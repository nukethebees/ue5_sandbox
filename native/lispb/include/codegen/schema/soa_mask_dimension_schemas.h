#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct SoaMaskDimensionSchemas {
    [[nodiscard]] auto append() -> SoaMaskDimensionId;
    [[nodiscard]] auto clone(SoaMaskDimensionId id) -> SoaMaskDimensionId;
    [[nodiscard]] auto index(SoaMaskDimensionId id) const -> std::uint32_t;
    void clear();

    std::vector<StringId> index_name;
    std::vector<StringId> extent;
};

}
