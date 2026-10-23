#include <codegen/lowering/lowering_storage.h>
namespace lispb::codegen {
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
auto LoweringStorage::save(std::span<StringId const> values) -> SchemaRange<StringId> {
    SchemaRange<StringId> const result{static_cast<std::uint32_t>(identifiers_.size()),
                                       static_cast<std::uint32_t>(values.size())};
    identifiers_.insert(identifiers_.end(), values.begin(), values.end());
    return result;
}
auto LoweringStorage::items(SchemaRange<StringId> range) const -> std::span<StringId const> {
    return std::span{identifiers_}.subspan(range.offset, range.count);
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
    equivalent_constructors.clear();
    single_allocation_columns.clear();
    paths.clear();
    columns_.clear();
    identifiers_.clear();
}
}
