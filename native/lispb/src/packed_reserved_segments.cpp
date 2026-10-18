#include <lispb/schema/packed_reserved_segments.h>

#include <cassert>

namespace lispb::schema {

auto PackedReservedSegments::append() -> PackedSegmentId {
    auto const row{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    bit_width.push_back({});
    return PackedSegmentId{(static_cast<std::uint32_t>(PackedSegmentKind::reserved) << 24) | row};
}
auto PackedReservedSegments::index(PackedSegmentId const id) const -> std::uint32_t {
    assert(id.kind() == PackedSegmentKind::reserved);
    auto const row{id.index()};
    assert(row < name.size());
    return row;
}

} // namespace lispb::schema
