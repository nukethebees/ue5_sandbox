#include <lispb/schema/integer_varint_types.h>

#include <cassert>

namespace lispb::schema {

auto IntegerVarintTypes::append() -> TypeDefinitionId {
    auto const row{static_cast<std::uint32_t>(source.size())};
    source.push_back({});
    encoding.push_back(codegen::IntegerVarintEncoding::unsigned_varint);
    return TypeDefinitionId{(static_cast<std::uint32_t>(TypeDefinitionKind::integer_varint) << 24) |
                            row};
}
auto IntegerVarintTypes::index(TypeDefinitionId const id) const -> std::uint32_t {
    assert(id.kind() == TypeDefinitionKind::integer_varint);
    auto const row{id.index()};
    assert(row < source.size());
    return row;
}

} // namespace lispb::schema
