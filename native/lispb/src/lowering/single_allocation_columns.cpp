#include <codegen/lowering/single_allocation_columns.h>

#include <cassert>
namespace lispb::codegen {
auto SingleAllocationColumns::append() -> SingleAllocationColumnId {
    SingleAllocationColumnId const id{static_cast<std::uint32_t>(flattened_identifier.size() + 1)};
    flattened_identifier.emplace_back();
    layout_identifier.emplace_back();
    type.emplace_back();
    member_path.emplace_back();
    return id;
}
auto SingleAllocationColumns::index(SingleAllocationColumnId id) const -> std::uint32_t {
    assert(id.value > 0 && id.value <= flattened_identifier.size());
    return id.value - 1;
}
void SingleAllocationColumns::clear() {
    flattened_identifier.clear();
    layout_identifier.clear();
    type.clear();
    member_path.clear();
}
}
