#pragma once
#include <codegen/lowering/indices.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen {
struct FixedMemberLayouts {
    auto append() -> FixedMemberLayoutId;
    auto index(FixedMemberLayoutId id) const -> std::uint32_t;
    void clear();
    std::vector<SoaMemberId> source;
    std::vector<ResolvedMemberId> member;
    std::vector<SchemaRange<FixedLeafId>> leaves;
    std::vector<StringId> nested_storage_name;
};
}
