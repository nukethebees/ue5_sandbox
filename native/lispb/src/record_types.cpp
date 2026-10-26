#include <lispb/schema/record_types.h>

#include <cassert>

namespace lispb::schema {

auto RecordTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(members.size())};
    members.push_back({});
    layout.push_back(codegen::LayoutKind::native);
    bits.emplace_back();
    storage_type.emplace_back();
    invalid_raw_value.emplace_back();
    byte_order.emplace_back();
    bit_order.push_back(codegen::PackedBitOrder::least_significant_first);
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::record) << 24) | row};
}
auto RecordTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::record);
    auto const row{id.index()};
    assert(row < members.size());
    return row;
}
auto RecordTypes::members_of(TypeDefinitionId const id) const
    -> std::span<ChildOccurrenceId const> {
    auto const range{members[index(id)]};
    return std::span{members_values}.subspan(range.begin, range.count);
}
void RecordTypes::set_members(TypeDefinitionId const id,
                              std::span<ChildOccurrenceId const> const values) {
    members[index(id)] = {static_cast<std::uint32_t>(members_values.size()),
                          static_cast<std::uint32_t>(values.size())};
    members_values.insert(members_values.end(), values.begin(), values.end());
}

} // namespace lispb::schema
