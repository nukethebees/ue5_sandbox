#include <lispb/schema/facade_types.h>

#include <cassert>

namespace lispb::schema {

auto FacadeTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(source.size())};
    source.push_back({});
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::facade) << 24) | row};
}
auto FacadeTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::facade);
    auto const row{id.index()};
    assert(row < source.size());
    return row;
}

} // namespace lispb::schema
