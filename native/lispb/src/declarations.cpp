#include <codegen/schema/declarations.h>

#include <cassert>
#include <utility>

namespace lispb::codegen {

auto Declarations::clone(DeclarationHandle id) -> DeclarationHandle {
    switch (id.kind()) {
        case DeclarationKind::enumeration:
            return enums.clone(id);
        case DeclarationKind::facade:
            return facades.clone(id);
        case DeclarationKind::fixed_point:
            return fixed_points.clone(id);
        case DeclarationKind::integer_scalar:
            return integer_scalars.clone(id);
        case DeclarationKind::integer_varint:
            return integer_varints.clone(id);
        case DeclarationKind::linear_quantized:
            return linear_quantizeds.clone(id);
        case DeclarationKind::mini_float:
            return mini_floats.clone(id);
        case DeclarationKind::optional_presence_bit:
            return optional_presence_bits.clone(id);
        case DeclarationKind::optional_sentinel:
            return optional_sentinels.clone(id);
        case DeclarationKind::packed_value:
            return packed_values.clone(id);
        case DeclarationKind::record:
            return records.clone(id);
        case DeclarationKind::soa:
            return soas.clone(id);
        case DeclarationKind::static_table:
            return static_tables.clone(id);
        case DeclarationKind::tagged_union:
            return tagged_unions.clone(id);
        case DeclarationKind::union_type:
            return unions.clone(id);
    }
    assert(false);
    std::unreachable();
}
auto Declarations::clone(ModuleHandle id) -> ModuleHandle {
    switch (id.kind()) {
        case ModuleKind::normal:
            return normal_modules.clone(id);
        case ModuleKind::settings:
            return settings_modules.clone(id);
        case ModuleKind::umbrella:
            return umbrella_modules.clone(id);
    }
    assert(false);
    std::unreachable();
}
auto Declarations::clone(PackedSegmentId id) -> PackedSegmentId {
    switch (id.kind()) {
        case PackedSegmentKind::field:
            return packed_fields.clone(id);
        case PackedSegmentKind::reserved:
            return packed_reserved_bits.clone(id);
    }
    assert(false);
    std::unreachable();
}
auto Declarations::module_settings(ModuleHandle id) const -> ModuleSettingsId {
    switch (id.kind()) {
        case ModuleKind::normal:
            return normal_modules.settings[normal_modules.index(id)];
        case ModuleKind::settings:
            return settings_modules.settings[settings_modules.index(id)];
        case ModuleKind::umbrella:
            return umbrella_modules.settings[umbrella_modules.index(id)];
    }
    assert(false);
    std::unreachable();
}
void Declarations::clear() {
    enumerators.clear();
    enums.clear();
    enum_unreal_projections.clear();
    facade_methods.clear();
    facades.clear();
    fixed_points.clear();
    functions.clear();
    integer_scalars.clear();
    integer_varints.clear();
    linear_quantizeds.clear();
    mini_floats.clear();
    module_settings_store.clear();
    normal_modules.clear();
    optional_presence_bits.clear();
    optional_sentinels.clear();
    packed_named_codes.clear();
    packed_fields.clear();
    packed_reserved_bits.clear();
    packed_values.clear();
    parameters.clear();
    record_members.clear();
    records.clear();
    semantic_relations.clear();
    setting_categories.clear();
    setting_controls.clear();
    settings.clear();
    settings_modules.clear();
    soa_mask_dimensions.clear();
    soa_members.clear();
    soas.clear();
    static_table_columns.clear();
    static_table_groups.clear();
    static_table_rows.clear();
    static_tables.clear();
    tagged_union_alternatives.clear();
    tagged_unions.clear();
    type_refs.clear();
    umbrella_modules.clear();
    union_alternatives.clear();
    unions.clear();
    type_ref_items_.clear();
    declaration_handle_items_.clear();
    enum_conversion_items_.clear();
    enumerator_id_items_.clear();
    facade_method_id_items_.clear();
    function_schema_id_items_.clear();
    native_enum_helper_items_.clear();
    packed_named_code_id_items_.clear();
    packed_segment_id_items_.clear();
    parameter_id_items_.clear();
    record_member_id_items_.clear();
    setting_category_id_items_.clear();
    setting_id_items_.clear();
    soa_mask_dimension_id_items_.clear();
    soa_member_id_items_.clear();
    static_table_column_id_items_.clear();
    static_table_group_id_items_.clear();
    static_table_row_id_items_.clear();
    tagged_union_alternative_id_items_.clear();
    type_ref_id_items_.clear();
    union_alternative_id_items_.clear();
    string_id_items_.clear();
}

}
