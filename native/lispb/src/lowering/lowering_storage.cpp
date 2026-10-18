#include <codegen/lowering/lowering_storage.h>
namespace lispb::codegen {
auto LoweringStorage::save(std::span<ResolvedMemberId const> values)
    -> SchemaRange<ResolvedMemberId> {
    SchemaRange<ResolvedMemberId> const result{static_cast<std::uint32_t>(resolved_members_.size()),
                                               static_cast<std::uint32_t>(values.size())};
    resolved_members_.insert(resolved_members_.end(), values.begin(), values.end());
    return result;
}
auto LoweringStorage::items(SchemaRange<ResolvedMemberId> range) const
    -> std::span<ResolvedMemberId const> {
    return std::span{resolved_members_}.subspan(range.offset, range.count);
}
auto LoweringStorage::save(std::span<FixedLeafId const> values) -> SchemaRange<FixedLeafId> {
    SchemaRange<FixedLeafId> const result{static_cast<std::uint32_t>(fixed_leaves_.size()),
                                          static_cast<std::uint32_t>(values.size())};
    fixed_leaves_.insert(fixed_leaves_.end(), values.begin(), values.end());
    return result;
}
auto LoweringStorage::items(SchemaRange<FixedLeafId> range) const -> std::span<FixedLeafId const> {
    return std::span{fixed_leaves_}.subspan(range.offset, range.count);
}
auto LoweringStorage::save(std::span<FixedMemberLayoutId const> values)
    -> SchemaRange<FixedMemberLayoutId> {
    SchemaRange<FixedMemberLayoutId> const result{static_cast<std::uint32_t>(fixed_members_.size()),
                                                  static_cast<std::uint32_t>(values.size())};
    fixed_members_.insert(fixed_members_.end(), values.begin(), values.end());
    return result;
}
auto LoweringStorage::items(SchemaRange<FixedMemberLayoutId> range) const
    -> std::span<FixedMemberLayoutId const> {
    return std::span{fixed_members_}.subspan(range.offset, range.count);
}
auto LoweringStorage::save(std::span<SingleAllocationColumnId const> values)
    -> SchemaRange<SingleAllocationColumnId> {
    SchemaRange<SingleAllocationColumnId> const result{static_cast<std::uint32_t>(columns_.size()),
                                                       static_cast<std::uint32_t>(values.size())};
    columns_.insert(columns_.end(), values.begin(), values.end());
    return result;
}
auto LoweringStorage::items(SchemaRange<SingleAllocationColumnId> range) const
    -> std::span<SingleAllocationColumnId const> {
    return std::span{columns_}.subspan(range.offset, range.count);
}
auto LoweringStorage::save(std::span<IdentifierId const> values) -> SchemaRange<IdentifierId> {
    SchemaRange<IdentifierId> const result{static_cast<std::uint32_t>(identifiers_.size()),
                                           static_cast<std::uint32_t>(values.size())};
    identifiers_.insert(identifiers_.end(), values.begin(), values.end());
    return result;
}
auto LoweringStorage::items(SchemaRange<IdentifierId> range) const
    -> std::span<IdentifierId const> {
    return std::span{identifiers_}.subspan(range.offset, range.count);
}
auto LoweringStorage::save(std::span<RowParameterId const> values) -> SchemaRange<RowParameterId> {
    SchemaRange<RowParameterId> const result{static_cast<std::uint32_t>(row_parameters_.size()),
                                             static_cast<std::uint32_t>(values.size())};
    row_parameters_.insert(row_parameters_.end(), values.begin(), values.end());
    return result;
}
auto LoweringStorage::items(SchemaRange<RowParameterId> range) const
    -> std::span<RowParameterId const> {
    return std::span{row_parameters_}.subspan(range.offset, range.count);
}
auto LoweringStorage::save(std::span<TypeDependencyId const> values)
    -> SchemaRange<TypeDependencyId> {
    SchemaRange<TypeDependencyId> const range{static_cast<std::uint32_t>(dependencies_.size()),
                                              static_cast<std::uint32_t>(values.size())};
    dependencies_.insert(dependencies_.end(), values.begin(), values.end());
    return range;
}
auto LoweringStorage::items(SchemaRange<TypeDependencyId> range) const
    -> std::span<TypeDependencyId const> {
    return std::span{dependencies_}.subspan(range.offset, range.count);
}
void LoweringStorage::clear() {
    packed_field_layouts.clear();
    dependencies_.clear();
    single_allocation_models.clear();
    single_allocation_views.clear();
    view_lookup.clear();
    column_lookup.clear();
    compact_vector_lookup.clear();
    equivalent_constructors.clear();
    resolved_members.clear();
    fixed_leaves.clear();
    fixed_member_layouts.clear();
    fixed_layouts.clear();
    single_allocation_columns.clear();
    compact_vectors.clear();
    row_parameters.clear();
    paths.clear();
    resolved_members_.clear();
    fixed_leaves_.clear();
    fixed_members_.clear();
    columns_.clear();
    identifiers_.clear();
    row_parameters_.clear();
}
}
