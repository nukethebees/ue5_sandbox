#include <codegen/schema/packed_value_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto PackedValueSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto PackedValueSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    storage_type.push_back({});
    segments.push_back({});
    invalid_value.push_back({});
    export_specifier.push_back({});
    mutable_value.push_back(false);
    byte_order.push_back({});
    bit_order.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::packed_value) << 24) |
                             (index + 1)};
}
auto PackedValueSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    storage_type[destination] = storage_type[source_index];
    segments[destination] = segments[source_index];
    invalid_value[destination] = invalid_value[source_index];
    export_specifier[destination] = export_specifier[source_index];
    mutable_value[destination] = mutable_value[source_index];
    byte_order[destination] = byte_order[source_index];
    bit_order[destination] = bit_order[source_index];
    return result;
}
void PackedValueSchemas::clear() {
    name.clear();
    storage_type.clear();
    segments.clear();
    invalid_value.clear();
    export_specifier.clear();
    mutable_value.clear();
    byte_order.clear();
    bit_order.clear();
}

}
