#pragma once
#include <codegen/ast/indices.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen {
struct SingleAllocationModels {
    auto append() -> SingleAllocationModelId;
    auto index(SingleAllocationModelId id) const -> std::uint32_t;
    void clear();
    std::vector<DeclarationHandle> schema;
    std::vector<StringId> owner_name;
    std::vector<StringId> layout_name;
    std::vector<StringId> view_name;
    std::vector<StringId> const_view_name;
    std::vector<SchemaRange<SingleAllocationColumnId>> columns;
    std::vector<SchemaRange<TypeDependencyId>> dependencies;
};
}
