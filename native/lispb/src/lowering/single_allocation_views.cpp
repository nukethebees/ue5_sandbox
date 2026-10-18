#include <codegen/lowering/single_allocation_views.h>

#include <cassert>
namespace lispb::codegen {
auto SingleAllocationViews::append() -> SingleAllocationViewId {
    SingleAllocationViewId const id{static_cast<std::uint32_t>(model.size() + 1)};
    model.emplace_back();
    schema.emplace_back();
    member_prefix.emplace_back();
    view_name.emplace_back();
    const_view_name.emplace_back();
    return id;
}
auto SingleAllocationViews::index(SingleAllocationViewId id) const -> std::uint32_t {
    assert(id.value > 0 && id.value <= model.size());
    return id.value - 1;
}
void SingleAllocationViews::clear() {
    model.clear();
    schema.clear();
    member_prefix.clear();
    view_name.clear();
    const_view_name.clear();
}
}
