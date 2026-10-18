#include <codegen/schema/soa_allocators.h>

#include <cassert>

namespace lispb::codegen {

auto SoaAllocators::index(SoaAllocatorVariantId id) const -> std::uint32_t {
    assert(id.value != 0);
    auto const row{id.value - 1};
    assert(row < prefix.size());
    return row;
}
auto SoaAllocators::append() -> SoaAllocatorVariantId {
    auto const index{static_cast<std::uint32_t>(prefix.size())};
    prefix.push_back({});
    allocator.push_back({});
    return SoaAllocatorVariantId{index + 1};
}
auto SoaAllocators::clone(SoaAllocatorVariantId id) -> SoaAllocatorVariantId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    prefix[destination] = prefix[source_index];
    allocator[destination] = allocator[source_index];
    return result;
}
void SoaAllocators::clear() {
    prefix.clear();
    allocator.clear();
}

}
