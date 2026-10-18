#pragma once
#include <codegen/lowering/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/schema_range.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen {
struct FixedLayouts {
    auto append() -> FixedLayoutId;
    auto index(FixedLayoutId id) const -> std::uint32_t;
    void clear();
    std::vector<DeclarationHandle> schema;
    std::vector<SchemaRange<FixedMemberLayoutId>> members;
    std::vector<SchemaRange<FixedLeafId>> leaves;
};
}
