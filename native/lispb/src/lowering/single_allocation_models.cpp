#include <codegen/lowering/single_allocation_models.h>

#include <cassert>
namespace lispb::codegen {
auto SingleAllocationModels::append() -> SingleAllocationModelId {
    SingleAllocationModelId const id{static_cast<std::uint32_t>(schema.size() + 1)};
    schema.emplace_back();
    backend.emplace_back();
    owner_name.emplace_back();
    layout_name.emplace_back();
    view_name.emplace_back();
    const_view_name.emplace_back();
    runtime_namespace.emplace_back();
    vector_namespace.emplace_back();
    size_type.emplace_back();
    byte_size_type.emplace_back();
    span_template.emplace_back();
    column_iteration_function.emplace_back();
    column_iteration_returns_result.emplace_back();
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
    backend.clear();
    owner_name.clear();
    layout_name.clear();
    view_name.clear();
    const_view_name.clear();
    runtime_namespace.clear();
    vector_namespace.clear();
    size_type.clear();
    byte_size_type.clear();
    span_template.clear();
    column_iteration_function.clear();
    column_iteration_returns_result.clear();
    columns.clear();
    dependencies.clear();
}
}
