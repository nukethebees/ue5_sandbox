#include <lispb/schema/fixed_point_types.h>

#include <cassert>

namespace lispb::schema {
auto FixedPointTypes::append(codegen::FixedPointFormat const value,
                             codegen::FixedPointRounding const rounding_value) -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(format.size())};
    format.push_back(value);
    rounding.push_back(rounding_value);
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::fixed_point) << 24) |
                            row};
}
auto FixedPointTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::fixed_point);
    auto const row{id.index()};
    assert(row < format.size());
    return row;
}
}
