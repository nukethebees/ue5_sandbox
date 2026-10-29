#include <codegen/schema/soa_mask_dimension_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto SoaMaskDimensionSchemas::index(SoaMaskDimensionId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < index_name.size());
    return row;
}
auto SoaMaskDimensionSchemas::append() -> SoaMaskDimensionId {
    auto const index{static_cast<std::uint32_t>(index_name.size())};
    index_name.push_back({});
    extent.push_back({});
    return SoaMaskDimensionId{index + 1};
}
auto SoaMaskDimensionSchemas::clone(SoaMaskDimensionId id) -> SoaMaskDimensionId {
    return detail::clone_schema_row(*this, id, index_name, extent);
}
void SoaMaskDimensionSchemas::clear() {
    detail::clear_schema_columns(index_name, extent);
}

}
