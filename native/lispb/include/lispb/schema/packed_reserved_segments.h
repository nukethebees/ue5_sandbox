#pragma once

#include <lispb/indices.h>
#include <lispb/schema/packed_segment_id.h>

#include <cstdint>
#include <vector>

namespace lispb::schema {

struct PackedReservedSegments {
    [[nodiscard]] auto append() -> PackedSegmentId;
    [[nodiscard]] auto index(PackedSegmentId id) const -> std::uint32_t;

    std::vector<IdentifierId> name;
    std::vector<std::uint32_t> bit_width;
};

} // namespace lispb::schema
