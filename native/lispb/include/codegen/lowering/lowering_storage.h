#pragma once
#include <codegen/lowering/compact_vectors.h>
#include <codegen/lowering/fixed_layouts.h>
#include <codegen/lowering/fixed_leaves.h>
#include <codegen/lowering/fixed_member_layouts.h>
#include <codegen/lowering/lowering_paths.h>
#include <codegen/lowering/packed_field_layouts.h>
#include <codegen/lowering/resolved_members.h>
#include <codegen/lowering/row_parameters.h>
#include <codegen/lowering/single_allocation_columns.h>
#include <codegen/lowering/single_allocation_models.h>
#include <codegen/lowering/single_allocation_views.h>

#include <map>
#include <span>
#include <utility>
#include <vector>
namespace lispb::codegen {
class LoweringStorage {
  public:
    PackedFieldLayouts packed_field_layouts;
    SingleAllocationModels single_allocation_models;
    SingleAllocationViews single_allocation_views;
    std::map<std::pair<SingleAllocationModelId, LoweringPathId>, SingleAllocationViewId>
        view_lookup;
    std::map<std::pair<SingleAllocationModelId, LoweringPathId>, SingleAllocationColumnId>
        column_lookup;
    std::map<std::pair<SingleAllocationModelId, LoweringPathId>, CompactVectorId>
        compact_vector_lookup;
    std::map<std::pair<SingleAllocationModelId, DeclarationHandle>, StringId>
        equivalent_constructors;
    ResolvedMembers resolved_members;
    FixedLeaves fixed_leaves;
    FixedMemberLayouts fixed_member_layouts;
    FixedLayouts fixed_layouts;
    SingleAllocationColumns single_allocation_columns;
    CompactVectors compact_vectors;
    RowParameters row_parameters;
    LoweringPaths paths;
    auto save(std::span<ResolvedMemberId const> values) -> SchemaRange<ResolvedMemberId>;
    auto items(SchemaRange<ResolvedMemberId> range) const -> std::span<ResolvedMemberId const>;
    auto save(std::span<FixedLeafId const> values) -> SchemaRange<FixedLeafId>;
    auto items(SchemaRange<FixedLeafId> range) const -> std::span<FixedLeafId const>;
    auto save(std::span<FixedMemberLayoutId const> values) -> SchemaRange<FixedMemberLayoutId>;
    auto items(SchemaRange<FixedMemberLayoutId> range) const
        -> std::span<FixedMemberLayoutId const>;
    auto save(std::span<SingleAllocationColumnId const> values)
        -> SchemaRange<SingleAllocationColumnId>;
    auto items(SchemaRange<SingleAllocationColumnId> range) const
        -> std::span<SingleAllocationColumnId const>;
    auto save(std::span<StringId const> values) -> SchemaRange<StringId>;
    auto items(SchemaRange<StringId> range) const -> std::span<StringId const>;
    auto save(std::span<RowParameterId const> values) -> SchemaRange<RowParameterId>;
    auto items(SchemaRange<RowParameterId> range) const -> std::span<RowParameterId const>;
    auto save(std::span<TypeDependencyId const> values) -> SchemaRange<TypeDependencyId>;
    auto items(SchemaRange<TypeDependencyId> range) const -> std::span<TypeDependencyId const>;
    void clear();
  private:
    std::vector<TypeDependencyId> dependencies_;
    std::vector<ResolvedMemberId> resolved_members_;
    std::vector<FixedLeafId> fixed_leaves_;
    std::vector<FixedMemberLayoutId> fixed_members_;
    std::vector<SingleAllocationColumnId> columns_;
    std::vector<StringId> identifiers_;
    std::vector<RowParameterId> row_parameters_;
};
}
