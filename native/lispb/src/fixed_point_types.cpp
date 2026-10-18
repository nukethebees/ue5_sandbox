#include <lispb/schema/fixed_point_types.h>

#include <cassert>

namespace lispb::schema {

auto FixedPointTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(signedness.size())};
    signedness.push_back({});
    total_bits.push_back({});
    fractional_bits.push_back({});
    rounding.push_back(codegen::FixedPointRounding::nearest_even);
    minimum_raw_value.push_back({});
    maximum_raw_value.push_back({});
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::fixed_point) << 24) |
                            row};
}
auto FixedPointTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::fixed_point);
    auto const row{id.index()};
    assert(row < signedness.size());
    return row;
}

} // namespace lispb::schema
