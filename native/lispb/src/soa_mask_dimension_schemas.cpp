#include <codegen/schema/soa_mask_dimension_schemas.h>

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
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    index_name[destination] = index_name[source_index];
    extent[destination] = extent[source_index];
    return result;
}
void SoaMaskDimensionSchemas::clear() {
    index_name.clear();
    extent.clear();
}

}
