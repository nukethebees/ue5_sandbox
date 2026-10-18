#include <codegen/schema/integer_varint_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto IntegerVarintSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    assert(id.kind() == DeclarationKind::integer_varint);
    auto const row{id.index()};
    assert(row < name.size());
    return row;
}
auto IntegerVarintSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    source.push_back({});
    encoding.push_back(IntegerVarintEncoding::unsigned_varint);
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::integer_varint) << 24) |
                             (index + 1)};
}
auto IntegerVarintSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    source[destination] = source[source_index];
    encoding[destination] = encoding[source_index];
    return result;
}
void IntegerVarintSchemas::clear() {
    name.clear();
    source.clear();
    encoding.clear();
}

}
