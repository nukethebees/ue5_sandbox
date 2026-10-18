#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct SoaAllocators {
    [[nodiscard]] auto append() -> SoaAllocatorVariantId;
    [[nodiscard]] auto clone(SoaAllocatorVariantId id) -> SoaAllocatorVariantId;
    [[nodiscard]] auto index(SoaAllocatorVariantId id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> prefix;
    std::vector<TypeRefId> allocator;
};

}
