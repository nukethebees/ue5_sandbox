#include <lispb/schema/soa_columns.h>

#include <cassert>

namespace lispb::schema {

auto SoaColumns::append() -> SoaColumnId {
    auto const row{static_cast<std::uint32_t>(source.size())};
    source.push_back({});
    name.push_back({});
    semantic_type.push_back({});
    kind.push_back(codegen::SoaMemberKind::array);
    nested_type.push_back({});
    relationship.push_back({});
    return SoaColumnId{row};
}
auto SoaColumns::index(SoaColumnId const id) const -> std::uint32_t {
    auto const row{id.value};
    assert(row < source.size());
    return row;
}

} // namespace lispb::schema
