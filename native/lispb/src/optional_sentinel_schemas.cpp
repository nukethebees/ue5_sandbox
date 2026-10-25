#include <codegen/schema/optional_sentinel_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto OptionalSentinelSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto OptionalSentinelSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    source.push_back({});
    sentinel.push_back({});
    return DeclarationHandle{
        (static_cast<std::uint32_t>(DeclarationKind::optional_sentinel) << 24) | (index + 1)};
}
auto OptionalSentinelSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    source[destination] = source[source_index];
    sentinel[destination] = sentinel[source_index];
    return result;
}
void OptionalSentinelSchemas::clear() {
    name.clear();
    source.clear();
    sentinel.clear();
}

}
