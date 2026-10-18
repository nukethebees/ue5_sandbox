#include <lispb/schema/enum_types.h>

#include <cassert>

namespace lispb::schema {

auto EnumTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(underlying_type.size())};
    underlying_type.push_back({});
    domain.push_back({});
    bit_width.push_back({});
    signedness.push_back(EnumSignedness::unsigned_);
    count.push_back({});
    enum_array.push_back({});
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::enumeration) << 24) |
                            row};
}
auto EnumTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::enumeration);
    auto const row{id.index()};
    assert(row < underlying_type.size());
    return row;
}

} // namespace lispb::schema
