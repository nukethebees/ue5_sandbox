#include <codegen/schema/optional_presence_bit_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto OptionalPresenceBitSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto OptionalPresenceBitSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    cpp_projection.push_back({});
    name.push_back({});
    source.push_back({});
    return DeclarationHandle{
        (static_cast<std::uint32_t>(DeclarationKind::optional_presence_bit) << 24) | (index + 1)};
}
auto OptionalPresenceBitSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    cpp_projection[destination] = cpp_projection[source_index];
    name[destination] = name[source_index];
    source[destination] = source[source_index];
    return result;
}
void OptionalPresenceBitSchemas::clear() {
    cpp_projection.clear();
    name.clear();
    source.clear();
}

}
