#include <lispb/schema/mini_float_types.h>

#include <cassert>

namespace lispb::schema {

auto MiniFloatTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(sign_bits.size())};
    sign_bits.push_back({});
    exponent_bits.push_back({});
    significand_bits.push_back({});
    exponent_bias.push_back({});
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::mini_float) << 24) |
                            row};
}
auto MiniFloatTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::mini_float);
    auto const row{id.index()};
    assert(row < sign_bits.size());
    return row;
}

} // namespace lispb::schema
