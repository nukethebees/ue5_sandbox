#include <codegen/schema/packed_reserved_bits_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto PackedReservedBitsSchemas::index(PackedSegmentId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto PackedReservedBitsSchemas::append() -> PackedSegmentId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    bits.push_back({});
    return PackedSegmentId{(static_cast<std::uint32_t>(PackedSegmentKind::reserved) << 24) |
                           (index + 1)};
}
auto PackedReservedBitsSchemas::clone(PackedSegmentId id) -> PackedSegmentId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    bits[destination] = bits[source_index];
    return result;
}
void PackedReservedBitsSchemas::clear() {
    name.clear();
    bits.clear();
}

}
