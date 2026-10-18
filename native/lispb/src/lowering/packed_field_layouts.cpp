#include <codegen/lowering/packed_field_layouts.h>

#include <cassert>

namespace lispb::codegen {

void PackedFieldLayouts::prepare(std::size_t field_count) {
    type.resize(field_count);
    definition.resize(field_count);
    boolean.resize(field_count);
    offset.resize(field_count);
    value_mask.resize(field_count);
}
auto PackedFieldLayouts::index(schema::PackedSegmentId field) const -> std::uint32_t {
    assert(field.kind() == schema::PackedSegmentKind::field && field.index() < type.size());
    return field.index();
}
void PackedFieldLayouts::clear() {
    type.clear();
    definition.clear();
    boolean.clear();
    offset.clear();
    value_mask.clear();
}

}
