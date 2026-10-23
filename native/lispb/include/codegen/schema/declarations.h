#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enum_unreal_projections.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/facade_method_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/fixed_soa_schemas.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/homogeneous_value_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_kind.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/native_enum_helper.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_reserved_bits_schemas.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_segment_kind.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/setting_category_schemas.h>
#include <codegen/schema/setting_control_schemas.h>
#include <codegen/schema/setting_schemas.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/settings_schema_schemas.h>
#include <codegen/schema/soa_allocators.h>
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/static_table_column_schemas.h>
#include <codegen/schema/static_table_group_schemas.h>
#include <codegen/schema/static_table_row_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/storage_operation.h>
#include <codegen/schema/tagged_union_alternative_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/umbrella_module_schemas.h>
#include <codegen/schema/union_alternative_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/schema/vector_soa_schemas.h>
#include <lispb/indices.h>

#include <cassert>
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace lispb {
class Strings;
}
namespace lispb::codegen {

class Declarations {
  public:
    explicit Declarations(Strings& strings)
        : strings_{&strings} {}
    auto strings() const -> Strings& { return *strings_; }
    // Clone a version before editing; published handles remain valid for document history.
    auto clone(DeclarationHandle id) -> DeclarationHandle;
    auto clone(ModuleHandle id) -> ModuleHandle;
    auto clone(PackedSegmentId id) -> PackedSegmentId;
    auto module_settings(ModuleHandle id) const -> ModuleSettingsId;
    void clear();
    template <typename Id>
    [[nodiscard]] auto items(SchemaRange<Id> range) const -> std::span<Id const> {
        return std::span<Id const>{item_storage<Id>()}.subspan(range.offset, range.count);
    }

    template <typename Id>
    [[nodiscard]] auto range(std::span<Id const> values) -> SchemaRange<Id> {
        auto& items{item_storage<Id>()};
        assert(items.size() + values.size() <= std::numeric_limits<std::uint32_t>::max());
        SchemaRange<Id> const result{static_cast<std::uint32_t>(items.size()),
                                     static_cast<std::uint32_t>(values.size())};
        items.insert(items.end(), values.begin(), values.end());
        return result;
    }

    template <typename Id>
    [[nodiscard]] auto range(std::vector<Id> const& values) -> SchemaRange<Id> {
        return range(std::span<Id const>{values});
    }

    template <typename Id>
    [[nodiscard]] auto range(std::initializer_list<Id> values) -> SchemaRange<Id> {
        return range(std::span<Id const>{values.begin(), values.size()});
    }

    EnumeratorSchemas enumerators;
    EnumSchemas enums;
    EnumUnrealProjections enum_unreal_projections;
    FacadeMethodSchemas facade_methods;
    FacadeSchemas facades;
    FixedPointSchemas fixed_points;
    FixedSoaSchemas fixed_soas;
    FunctionSchemas functions;
    HomogeneousLayoutSchemas homogeneous_layouts;
    HomogeneousValueSchemas homogeneous_values;
    IntegerScalarSchemas integer_scalars;
    IntegerVarintSchemas integer_varints;
    LinearQuantizedSchemas linear_quantizeds;
    MiniFloatSchemas mini_floats;
    ModuleSettingsStore module_settings_store;
    NormalModuleSchemas normal_modules;
    OptionalPresenceBitSchemas optional_presence_bits;
    OptionalSentinelSchemas optional_sentinels;
    PackedNamedCodeSchemas packed_named_codes;
    PackedFieldSchemas packed_fields;
    PackedReservedBitsSchemas packed_reserved_bits;
    PackedValueSchemas packed_values;
    ParameterSchemas parameters;
    RecordMemberSchemas record_members;
    RecordSchemas records;
    SemanticRelationSchemas semantic_relations;
    SettingCategorySchemas setting_categories;
    SettingControlSchemas setting_controls;
    SettingSchemas settings;
    SettingsModuleSchemas settings_modules;
    SettingsSchemaSchemas settings_schemas;
    SoaAllocators soa_allocator_variants;
    SoaMaskDimensionSchemas soa_mask_dimensions;
    SoaMemberSchemas soa_members;
    SoaSchemas soas;
    StaticTableColumnSchemas static_table_columns;
    StaticTableGroupSchemas static_table_groups;
    StaticTableRowSchemas static_table_rows;
    StaticTableSchemas static_tables;
    TaggedUnionAlternativeSchemas tagged_union_alternatives;
    TaggedUnionSchemas tagged_unions;
    TypeRefs type_refs;
    UmbrellaModuleSchemas umbrella_modules;
    UnionAlternativeSchemas union_alternatives;
    UnionSchemas unions;
    VectorSoaSchemas vector_soas;
  private:
    template <typename Id>
    auto item_storage() const -> std::vector<Id> const& {
        if constexpr (std::same_as<Id, DeclarationHandle>) {
            return declaration_handle_items_;
        } else if constexpr (std::same_as<Id, EnumConversion>) {
            return enum_conversion_items_;
        } else if constexpr (std::same_as<Id, EnumeratorId>) {
            return enumerator_id_items_;
        } else if constexpr (std::same_as<Id, FacadeMethodId>) {
            return facade_method_id_items_;
        } else if constexpr (std::same_as<Id, FunctionSchemaId>) {
            return function_schema_id_items_;
        } else if constexpr (std::same_as<Id, HomogeneousValueId>) {
            return homogeneous_value_id_items_;
        } else if constexpr (std::same_as<Id, NativeEnumHelper>) {
            return native_enum_helper_items_;
        } else if constexpr (std::same_as<Id, PackedNamedCodeId>) {
            return packed_named_code_id_items_;
        } else if constexpr (std::same_as<Id, PackedSegmentId>) {
            return packed_segment_id_items_;
        } else if constexpr (std::same_as<Id, ParameterId>) {
            return parameter_id_items_;
        } else if constexpr (std::same_as<Id, RecordMemberId>) {
            return record_member_id_items_;
        } else if constexpr (std::same_as<Id, SettingCategoryId>) {
            return setting_category_id_items_;
        } else if constexpr (std::same_as<Id, SettingId>) {
            return setting_id_items_;
        } else if constexpr (std::same_as<Id, SoaAllocatorVariantId>) {
            return soa_allocator_variant_id_items_;
        } else if constexpr (std::same_as<Id, SoaMaskDimensionId>) {
            return soa_mask_dimension_id_items_;
        } else if constexpr (std::same_as<Id, SoaMemberId>) {
            return soa_member_id_items_;
        } else if constexpr (std::same_as<Id, StaticTableColumnId>) {
            return static_table_column_id_items_;
        } else if constexpr (std::same_as<Id, StaticTableGroupId>) {
            return static_table_group_id_items_;
        } else if constexpr (std::same_as<Id, StaticTableRowId>) {
            return static_table_row_id_items_;
        } else if constexpr (std::same_as<Id, StorageOperation>) {
            return storage_operation_items_;
        } else if constexpr (std::same_as<Id, TaggedUnionAlternativeId>) {
            return tagged_union_alternative_id_items_;
        } else if constexpr (std::same_as<Id, TypeRefId>) {
            return type_ref_id_items_;
        } else if constexpr (std::same_as<Id, UnionAlternativeId>) {
            return union_alternative_id_items_;
        } else if constexpr (std::same_as<Id, StringId>) {
            return string_id_items_;
        } else {
            static_assert(sizeof(Id) == 0, "unsupported schema range");
        }
    }

    template <typename Id>
    auto item_storage() -> std::vector<Id>& {
        return const_cast<std::vector<Id>&>(std::as_const(*this).item_storage<Id>());
    }

    Strings* strings_;
    std::vector<DeclarationHandle> declaration_handle_items_;
    std::vector<EnumConversion> enum_conversion_items_;
    std::vector<EnumeratorId> enumerator_id_items_;
    std::vector<FacadeMethodId> facade_method_id_items_;
    std::vector<FunctionSchemaId> function_schema_id_items_;
    std::vector<HomogeneousValueId> homogeneous_value_id_items_;
    std::vector<NativeEnumHelper> native_enum_helper_items_;
    std::vector<PackedNamedCodeId> packed_named_code_id_items_;
    std::vector<PackedSegmentId> packed_segment_id_items_;
    std::vector<ParameterId> parameter_id_items_;
    std::vector<RecordMemberId> record_member_id_items_;
    std::vector<SettingCategoryId> setting_category_id_items_;
    std::vector<SettingId> setting_id_items_;
    std::vector<SoaAllocatorVariantId> soa_allocator_variant_id_items_;
    std::vector<SoaMaskDimensionId> soa_mask_dimension_id_items_;
    std::vector<SoaMemberId> soa_member_id_items_;
    std::vector<StaticTableColumnId> static_table_column_id_items_;
    std::vector<StaticTableGroupId> static_table_group_id_items_;
    std::vector<StaticTableRowId> static_table_row_id_items_;
    std::vector<StorageOperation> storage_operation_items_;
    std::vector<TaggedUnionAlternativeId> tagged_union_alternative_id_items_;
    std::vector<TypeRefId> type_ref_id_items_;
    std::vector<UnionAlternativeId> union_alternative_id_items_;
    std::vector<StringId> string_id_items_;
};

}
