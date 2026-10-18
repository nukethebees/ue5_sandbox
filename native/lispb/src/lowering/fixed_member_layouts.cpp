#include <codegen/lowering/fixed_member_layouts.h>

#include <cassert>
namespace lispb::codegen {
auto FixedMemberLayouts::append() -> FixedMemberLayoutId {
    FixedMemberLayoutId const id{static_cast<std::uint32_t>(source.size() + 1)};
    source.emplace_back();
    member.emplace_back();
    leaves.emplace_back();
    nested_storage_name.emplace_back();
    return id;
}
auto FixedMemberLayouts::index(FixedMemberLayoutId id) const -> std::uint32_t {
    assert(id.value > 0 && id.value <= source.size());
    return id.value - 1;
}
void FixedMemberLayouts::clear() {
    source.clear();
    member.clear();
    leaves.clear();
    nested_storage_name.clear();
}
}
