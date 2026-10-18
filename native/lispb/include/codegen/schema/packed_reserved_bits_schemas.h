#pragma once

#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_segment_kind.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct PackedReservedBitsSchemas {
    [[nodiscard]] auto append() -> PackedSegmentId;
    [[nodiscard]] auto clone(PackedSegmentId id) -> PackedSegmentId;
    [[nodiscard]] auto index(PackedSegmentId id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<int> bits;
};

}
