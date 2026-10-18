#pragma once
#include <codegen/lowering/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen {
struct SingleAllocationViews {
    auto append() -> SingleAllocationViewId;
    auto index(SingleAllocationViewId id) const -> std::uint32_t;
    void clear();
    std::vector<SingleAllocationModelId> model;
    std::vector<DeclarationHandle> schema;
    std::vector<LoweringPathId> member_prefix;
    std::vector<IdentifierId> view_name;
    std::vector<IdentifierId> const_view_name;
};
}
