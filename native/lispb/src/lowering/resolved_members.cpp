#include <codegen/lowering/resolved_members.h>

#include <cassert>
namespace lispb::codegen {
auto ResolvedMembers::append() -> ResolvedMemberId {
    ResolvedMemberId const id{static_cast<std::uint32_t>(source.size() + 1)};
    source.emplace_back();
    name.emplace_back();
    kind.emplace_back();
    element_type.emplace_back();
    container_type.emplace_back();
    view_type.emplace_back();
    const_view_type.emplace_back();
    return id;
}
auto ResolvedMembers::index(ResolvedMemberId id) const -> std::uint32_t {
    assert(id.value > 0 && id.value <= source.size());
    return id.value - 1;
}
void ResolvedMembers::clear() {
    source.clear();
    name.clear();
    kind.clear();
    element_type.clear();
    container_type.clear();
    view_type.clear();
    const_view_type.clear();
}
}
