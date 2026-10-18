#include <lispb/schema/packed_types.h>

#include <cassert>

namespace lispb::schema {

auto PackedTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(storage_type.size())};
    storage_type.push_back({});
    segments.push_back({});
    invalid_raw_value.push_back({});
    byte_order.push_back({});
    bit_order.push_back(codegen::PackedBitOrder::least_significant_first);
    default_raw_value.push_back({});
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::packed) << 24) | row};
}
auto PackedTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::packed);
    auto const row{id.index()};
    assert(row < storage_type.size());
    return row;
}
auto PackedTypes::segments_of(TypeDefinitionId const id) const -> std::span<PackedSegmentId const> {
    auto const range{segments[index(id)]};
    return std::span{segments_values}.subspan(range.begin, range.count);
}
void PackedTypes::set_segments(TypeDefinitionId const id,
                               std::span<PackedSegmentId const> const values) {
    segments[index(id)] = {static_cast<std::uint32_t>(segments_values.size()),
                           static_cast<std::uint32_t>(values.size())};
    segments_values.insert(segments_values.end(), values.begin(), values.end());
}

} // namespace lispb::schema
