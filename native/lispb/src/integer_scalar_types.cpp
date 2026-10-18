#include <lispb/schema/integer_scalar_types.h>

#include <cassert>

namespace lispb::schema {

auto IntegerScalarTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(signedness.size())};
    signedness.push_back({});
    minimum_value.push_back({});
    maximum_value.push_back({});
    bit_width.push_back({});
    bit_width_auto.push_back({});
    named_codes.push_back({});
    relationship.push_back({});
    cpp_representation.push_back({});
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::integer_scalar) << 24) |
                            row};
}
auto IntegerScalarTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::integer_scalar);
    auto const row{id.index()};
    assert(row < signedness.size());
    return row;
}

} // namespace lispb::schema
