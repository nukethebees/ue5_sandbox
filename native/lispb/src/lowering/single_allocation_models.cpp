#include <codegen/lowering/single_allocation_models.h>

#include <cassert>
namespace lispb::codegen {
auto SingleAllocationModels::append() -> SingleAllocationModelId {
    SingleAllocationModelId const id{static_cast<std::uint32_t>(schema.size() + 1)};
    schema.emplace_back();
    owner_name.emplace_back();
    layout_name.emplace_back();
    view_name.emplace_back();
    const_view_name.emplace_back();
    columns.emplace_back();
    dependencies.emplace_back();
    return id;
}
auto SingleAllocationModels::index(SingleAllocationModelId id) const -> std::uint32_t {
    assert(id.value > 0 && id.value <= schema.size());
    return id.value - 1;
}
void SingleAllocationModels::clear() {
    schema.clear();
    owner_name.clear();
    layout_name.clear();
    view_name.clear();
    const_view_name.clear();
    columns.clear();
    dependencies.clear();
}
}
