#include <codegen/schema/integer_varint_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto IntegerVarintSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto IntegerVarintSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
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
    cpp_projection[destination] = cpp_projection[source_index];
    name[destination] = name[source_index];
    source[destination] = source[source_index];
    encoding[destination] = encoding[source_index];
    return result;
}
void IntegerVarintSchemas::clear() {
    cpp_projection.clear();
    name.clear();
    source.clear();
    encoding.clear();
}

}
