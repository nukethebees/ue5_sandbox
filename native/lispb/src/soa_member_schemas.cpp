#include <codegen/schema/soa_member_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto SoaMemberSchemas::index(SoaMemberId id) const -> std::uint32_t {
    assert(id.value != 0);
    auto const row{id.value - 1};
    assert(row < name.size());
    return row;
}
auto SoaMemberSchemas::append() -> SoaMemberId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    kind.push_back({});
    type.push_back({});
    fixed_schema.push_back({});
    nested_schema.push_back({});
    mask_field.push_back(false);
    mask_dimensions.push_back({});
    relationship.push_back({});
    return SoaMemberId{index + 1};
}
auto SoaMemberSchemas::clone(SoaMemberId id) -> SoaMemberId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    kind[destination] = kind[source_index];
    type[destination] = type[source_index];
    fixed_schema[destination] = fixed_schema[source_index];
    nested_schema[destination] = nested_schema[source_index];
    mask_field[destination] = mask_field[source_index];
    mask_dimensions[destination] = mask_dimensions[source_index];
    relationship[destination] = relationship[source_index];
    return result;
}
void SoaMemberSchemas::clear() {
    name.clear();
    kind.clear();
    type.clear();
    fixed_schema.clear();
    nested_schema.clear();
    mask_field.clear();
    mask_dimensions.clear();
    relationship.clear();
}

}
