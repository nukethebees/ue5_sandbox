#include <lispb/schema/packed_fields.h>

#include <cassert>

namespace lispb::schema {

auto PackedFields::append() -> PackedSegmentId {
    auto const row{static_cast<std::uint32_t>(source.size())};
    source.push_back({});
    name.push_back({});
    semantic_type.push_back({});
    bit_width.push_back({});
    bit_width_auto.push_back({});
    kind.push_back(codegen::PackedFieldKind::unsigned_integer);
    range_helper.push_back({});
    minimum_value.push_back({});
    maximum_value.push_back({});
    named_codes.push_back({});
    relationship.push_back({});
    default_value.push_back({});
    return PackedSegmentId{(static_cast<std::uint32_t>(PackedSegmentKind::field) << 24) | row};
}
auto PackedFields::index(PackedSegmentId const id) const -> std::uint32_t {
    assert(id.kind() == PackedSegmentKind::field);
    auto const row{id.index()};
    assert(row < source.size());
    return row;
}

} // namespace lispb::schema
