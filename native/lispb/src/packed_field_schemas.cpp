#include <codegen/schema/packed_field_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto PackedFieldSchemas::index(PackedSegmentId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto PackedFieldSchemas::append() -> PackedSegmentId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    type.push_back({});
    bits.push_back({});
    kind.push_back(PackedFieldKind::unsigned_integer);
    range_helper.push_back(false);
    minimum_value.push_back({});
    maximum_value.push_back({});
    named_codes.push_back({});
    relationship.push_back({});
    default_value.push_back({});
    return PackedSegmentId{(static_cast<std::uint32_t>(PackedSegmentKind::field) << 24) |
                           (index + 1)};
}
auto PackedFieldSchemas::clone(PackedSegmentId id) -> PackedSegmentId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    type[destination] = type[source_index];
    bits[destination] = bits[source_index];
    kind[destination] = kind[source_index];
    range_helper[destination] = range_helper[source_index];
    minimum_value[destination] = minimum_value[source_index];
    maximum_value[destination] = maximum_value[source_index];
    named_codes[destination] = named_codes[source_index];
    relationship[destination] = relationship[source_index];
    default_value[destination] = default_value[source_index];
    return result;
}
void PackedFieldSchemas::clear() {
    name.clear();
    type.clear();
    bits.clear();
    kind.clear();
    range_helper.clear();
    minimum_value.clear();
    maximum_value.clear();
    named_codes.clear();
    relationship.clear();
    default_value.clear();
}

}
