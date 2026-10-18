#include <codegen/lowering/fixed_layouts.h>

#include <cassert>
namespace lispb::codegen {
auto FixedLayouts::append() -> FixedLayoutId {
    FixedLayoutId const id{static_cast<std::uint32_t>(schema.size() + 1)};
    schema.emplace_back();
    members.emplace_back();
    leaves.emplace_back();
    return id;
}
auto FixedLayouts::index(FixedLayoutId id) const -> std::uint32_t {
    assert(id.value > 0 && id.value <= schema.size());
    return id.value - 1;
}
void FixedLayouts::clear() {
    schema.clear();
    members.clear();
    leaves.clear();
}
}
