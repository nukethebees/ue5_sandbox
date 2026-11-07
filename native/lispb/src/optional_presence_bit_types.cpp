#include <lispb/schema/optional_presence_bit_types.h>

#include <cassert>

namespace lispb::schema {

auto OptionalPresenceBitTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(source.size())};
    source.push_back({});
    payload_bits.push_back({});
    encoded_bits.push_back({});
    return TypeDefinitionId{
        (static_cast<std::uint32_t>(TypeDefinitionKind::optional_presence_bit) << 24) | row};
}
auto OptionalPresenceBitTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::optional_presence_bit);
    auto const row{id.index()};
    assert(row < source.size());
    return row;
}

} // namespace lispb::schema
