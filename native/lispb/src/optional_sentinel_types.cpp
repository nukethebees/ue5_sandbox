#include <lispb/schema/optional_sentinel_types.h>

#include <cassert>

namespace lispb::schema {

auto OptionalSentinelTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(source.size())};
    source.push_back({});
    sentinel_name.push_back({});
    sentinel_value.push_back({});
    bit_width.push_back({});
    return TypeDefinitionId{
        (static_cast<std::uint32_t>(TypeDefinitionKind::optional_sentinel) << 24) | row};
}
auto OptionalSentinelTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::optional_sentinel);
    auto const row{id.index()};
    assert(row < source.size());
    return row;
}

} // namespace lispb::schema
