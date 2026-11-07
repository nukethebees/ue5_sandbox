#include <lispb/schema/mini_float_types.h>

#include <cassert>

namespace lispb::schema {
auto MiniFloatTypes::append(codegen::MiniFloatFormat const value) -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(format.size())};
    format.push_back(value);
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::mini_float) << 24) |
                            row};
}
auto MiniFloatTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::mini_float);
    auto const row{id.index()};
    assert(row < format.size());
    return row;
}
}
