#include <codegen/lowering/fixed_leaves.h>

#include <cassert>
namespace lispb::codegen {
auto FixedLeaves::append() -> FixedLeafId {
    FixedLeafId const id{static_cast<std::uint32_t>(path.size() + 1)};
    path.emplace_back();
    type.emplace_back();
    return id;
}
auto FixedLeaves::index(FixedLeafId id) const -> std::uint32_t {
    assert(id.value > 0 && id.value <= path.size());
    return id.value - 1;
}
void FixedLeaves::clear() {
    path.clear();
    type.clear();
}
}
