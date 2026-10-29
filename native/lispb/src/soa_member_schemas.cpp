#include <codegen/schema/soa_member_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto SoaMemberSchemas::index(SoaMemberId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto SoaMemberSchemas::append() -> SoaMemberId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    kind.push_back({});
    type.push_back({});
    equivalent_member.push_back({});
    equivalent_accessor.push_back(false);
    mask_field.push_back(false);
    mask_dimensions.push_back({});
    relationship.push_back({});
    return SoaMemberId{index + 1};
}
auto SoaMemberSchemas::clone(SoaMemberId id) -> SoaMemberId {
    return detail::clone_schema_row(*this,
                                    id,
                                    name,
                                    kind,
                                    type,
                                    equivalent_member,
                                    equivalent_accessor,
                                    mask_field,
                                    mask_dimensions,
                                    relationship);
}
void SoaMemberSchemas::clear() {
    detail::clear_schema_columns(name,
                                 kind,
                                 type,
                                 equivalent_member,
                                 equivalent_accessor,
                                 mask_field,
                                 mask_dimensions,
                                 relationship);
}

}
