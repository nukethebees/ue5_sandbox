#include "canonical_renderer.h"

#include <codegen/ast/access.h>
#include <codegen/ast/record_kind.h>
#include <codegen/schema.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enum_unreal_projections.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/facade_method_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/fixed_soa_schemas.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/homogeneous_value_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/native_enum_helper.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_reserved_bits_schemas.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/packed_segment_kind.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/schema/record_comparison.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <codegen/schema/soa_allocators.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/soa_storage.h>
#include <codegen/schema/static_table_column_schemas.h>
#include <codegen/schema/static_table_group_schemas.h>
#include <codegen/schema/static_table_row_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/storage_operation.h>
#include <codegen/schema/tagged_union_alternative_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/type_registry.h>
#include <codegen/schema/union_alternative_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <codegen/schema/vector_soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::schema::detail {

CanonicalRenderer::CanonicalRenderer(codegen::Declarations const& declarations)
    : store_{declarations}
    , ir_{declarations.strings()} {}
auto CanonicalRenderer::text(StringId value) const -> std::string_view {
    return store_.strings().view(value);
}
auto CanonicalRenderer::text(IdentifierId value) const -> std::string_view {
    return store_.strings().view(value);
}
auto CanonicalRenderer::render_declaration_schema(codegen::DeclarationHandle const declaration)
    -> std::string {
    return ir_.render(build_declaration_schema(declaration));
}
auto CanonicalRenderer::render_module(codegen::ModuleHandle const module) -> std::string {
    return ir_.render(build_module(module));
}
auto CanonicalRenderer::access_keyword(codegen::Access access) -> std::string_view {
    switch (access) {
        case codegen::Access::public_:
            return "public";
        case codegen::Access::private_:
            return "private";
        case codegen::Access::protected_:
            return "protected";
    }
    std::unreachable();
}
auto CanonicalRenderer::build_type_ref(codegen::TypeRefId const id) -> Expr {
    auto const& types{store_.type_refs};
    auto const index{types.index(id)};
    auto const name{text(types.name[index])};
    auto const needs_quotes{name.empty() || name.starts_with('#') ||
                            (name.starts_with(':') && !name.starts_with("::")) ||
                            std::ranges::any_of(name, [](unsigned char character) {
                                return std::isspace(character) != 0 || character == '(' ||
                                       character == ')' || character == '"' || character == ';';
                            })};
    auto const rendered_name{needs_quotes ? ir_.string(types.name[index])
                                          : ir_.atom(types.name[index])};
    if (text(types.suffix[index]).empty() && !types.nested[index].has_value()) {
        return rendered_name;
    }

    auto result{ir_.form("type-ref", {rendered_name})};
    if (!text(types.suffix[index]).empty()) {
        result.field("suffix", ir_.string(types.suffix[index]));
    }
    if (types.nested[index].has_value()) {
        result.field("nested", ir_.string(*types.nested[index]));
    }
    return result.build();
}
auto CanonicalRenderer::build_enum_value(codegen::EnumeratorId const value) -> Expr {
    auto const& enumerators{store_.enumerators};
    auto const index{enumerators.index(value)};
    auto result{ir_.form("value", {ir_.atom(enumerators.name[index])})};
    if (enumerators.initializer[index].has_value()) {
        result.field("value", ir_.string(*enumerators.initializer[index]));
    }
    if (enumerators.display_name[index].has_value()) {
        result.field("display-name", ir_.string(*enumerators.display_name[index]));
    }
    if (enumerators.hidden[index]) {
        result.field("hidden", ir_.atoms.true_value);
    }
    if (enumerators.serialized_name[index].has_value()) {
        result.field("serialized-name", ir_.string(*enumerators.serialized_name[index]));
    }
    if (enumerators.sentinel[index]) {
        result.field("sentinel", ir_.atoms.true_value);
    }
    return result.build();
}
auto CanonicalRenderer::build_enum_unreal_projection(
    codegen::EnumUnrealProjectionId const projection) -> Expr {
    auto const& projections{store_.enum_unreal_projections};
    auto const index{projections.index(projection)};
    auto result{ir_.form("unreal-projection", {ir_.atom(projections.name[index])})};
    result.field("header", ir_.string(projections.header[index]))
        .field("header-include", ir_.string(projections.header_include[index]))
        .field("conversion-header", ir_.string(projections.conversion_header[index]))
        .field("native-header-include", ir_.string(projections.native_header_include[index]));
    if (projections.enum_array[index]) {
        result.field("enum-array", ir_.atoms.true_value);
    }
    if (projections.reflection[index] != codegen::EnumReflection::uenum) {
        result.field("reflection",
                     ir_.atom(codegen::enum_reflection_name(projections.reflection[index])));
    }
    return result.build();
}
auto CanonicalRenderer::build_enum(codegen::DeclarationHandle const schema) -> Expr {
    auto const& enums{store_.enums};
    auto const index{enums.index(schema)};
    auto const name{ir_.atom(enums.name[index])};
    auto result{enums.underlying_type[index].has_value()
                    ? ir_.form("enum", {name, build_type_ref(*enums.underlying_type[index])})
                    : ir_.form("enum", {name})};
    if (enums.bit_width[index].has_value()) {
        result.field("bit-width", *enums.bit_width[index]);
    }
    if (enums.signedness[index] != EnumSignedness::unspecified) {
        result.field("signed",
                     enums.signedness[index] == EnumSignedness::signed_ ? ir_.atoms.true_value
                                                                        : ir_.atoms.false_value);
    }
    if (enums.reflection[index] != codegen::EnumReflection::none) {
        result.field("reflection",
                     ir_.atom(codegen::enum_reflection_name(enums.reflection[index])));
    }
    if (enums.enum_array[index]) {
        result.field("enum-array", ir_.atoms.true_value);
    }
    if (enums.count[index].has_value()) {
        result.field("count", ir_.atom(*enums.count[index]));
    }
    auto const conversions{items(enums.conversions[index])};
    if (!conversions.empty()) {
        std::vector<Expr> values;
        for (auto const conversion : conversions) {
            values.push_back(ir_.atom(codegen::enum_conversion_name(conversion)));
        }
        result.field("conversions", ir_.list(values));
    }
    if (enums.export_specifier[index].value != 0) {
        result.field("export-specifier", ir_.atom(enums.export_specifier[index]));
    }
    if (enums.native_api[index]) {
        result.field("native-api", ir_.atoms.true_value);
    }
    auto const helpers{items(enums.helpers[index])};
    if (!helpers.empty()) {
        std::vector<Expr> values;
        for (auto const helper : helpers) {
            values.push_back(ir_.atom(codegen::native_enum_helper_name(helper)));
        }
        result.field("helpers", ir_.list(values));
    }
    for (auto const value : items(enums.values[index])) {
        result.add(build_enum_value(value));
    }
    if (enums.unreal_projection[index].has_value()) {
        result.add(build_enum_unreal_projection(*enums.unreal_projection[index]));
    }
    return result.build();
}
auto CanonicalRenderer::packed_field_kind_name(codegen::PackedFieldKind const kind)
    -> std::string_view {
    switch (kind) {
        case codegen::PackedFieldKind::unsigned_integer:
            return "unsigned";
        case codegen::PackedFieldKind::signed_integer:
            return "signed";
        case codegen::PackedFieldKind::enumeration:
            return "enum";
        case codegen::PackedFieldKind::linear_quantized:
            return "linear-quantized";
        case codegen::PackedFieldKind::fixed_point:
            return "fixed-point";
        case codegen::PackedFieldKind::mini_float:
            return "mini-float";
    }
    return "unsigned";
}
auto CanonicalRenderer::soa_member_kind_name(codegen::SoaMemberKind const kind)
    -> std::string_view {
    switch (kind) {
        case codegen::SoaMemberKind::array:
            return "array";
        case codegen::SoaMemberKind::nested:
            return "nested";
    }
    return "array";
}
auto CanonicalRenderer::storage_operation_name(codegen::StorageOperation const operation)
    -> std::string_view {
    switch (operation) {
        case codegen::StorageOperation::reset:
            return "reset";
        case codegen::StorageOperation::reserve:
            return "reserve";
        case codegen::StorageOperation::add_uninitialised:
            return "add-uninitialised";
        case codegen::StorageOperation::add_defaulted:
            return "add-defaulted";
        case codegen::StorageOperation::remove_at_swap:
            return "remove-at-swap";
        case codegen::StorageOperation::set_num:
            return "set-num";
        case codegen::StorageOperation::copy_element:
            return "copy-element";
        case codegen::StorageOperation::append_from:
            return "append-from";
    }
    return "reset";
}
auto CanonicalRenderer::build_parameter(codegen::ParameterId const parameter) -> Expr {
    auto const& parameters{store_.parameters};
    auto const index{parameters.index(parameter)};
    auto result{ir_.form(
        "parameter", {ir_.atom(parameters.name[index]), build_type_ref(parameters.type[index])})};
    if (parameters.default_value[index].has_value()) {
        result.field("default", ir_.string(*parameters.default_value[index]));
    }
    return result.build();
}
auto CanonicalRenderer::build_function(codegen::FunctionSchemaId const function,
                                       std::string_view const kind) -> Expr {
    auto const& functions{store_.functions};
    auto const index{functions.index(function)};
    auto result{ir_.form(
        kind, {ir_.atom(functions.name[index]), build_type_ref(functions.return_type[index])})};
    if (!items(functions.body_lines[index]).empty()) {
        result.field("body", string_list(items(functions.body_lines[index])));
    }
    if (!items(functions.dependencies[index]).empty()) {
        result.field("dependencies", string_list(items(functions.dependencies[index])));
    }
    if (functions.trailing_return_type[index].has_value()) {
        result.field("trailing-return-type",
                     build_type_ref(*functions.trailing_return_type[index]));
    }
    if (functions.is_const[index]) {
        result.field("const", ir_.atoms.true_value);
    }
    if (functions.is_noexcept[index]) {
        result.field("noexcept", ir_.atoms.true_value);
    }
    if (functions.is_static[index]) {
        result.field("static", ir_.atoms.true_value);
    }
    if (functions.is_constexpr[index]) {
        result.field("constexpr", ir_.atoms.true_value);
    }
    if (functions.is_nodiscard[index]) {
        result.field("nodiscard", ir_.atoms.true_value);
    }
    if (functions.is_inline[index]) {
        result.field("inline", ir_.atoms.true_value);
    }
    if (functions.definition_in_source[index]) {
        result.field("definition-in-source", ir_.atoms.true_value);
    }
    if (functions.template_parameters[index].has_value()) {
        result.field("template-parameters", ir_.string(*functions.template_parameters[index]));
    }
    if (functions.requires_clause[index].has_value()) {
        result.field("requires", ir_.string(*functions.requires_clause[index]));
    }
    for (auto const parameter : items(functions.parameters[index])) {
        result.add(build_parameter(parameter));
    }
    return result.build();
}
auto CanonicalRenderer::build_semantic_relation(codegen::SemanticRelationId const relationship)
    -> Expr {
    auto const& relations{store_.semantic_relations};
    auto const index{relations.index(relationship)};
    auto result{ir_.form("relation",
                         {ir_.atom(semantic_relation_kind_name(relations.kind[index])),
                          build_type_ref(relations.target[index])})};
    if (relations.unit[index].has_value()) {
        result.field("unit", ir_.atom(semantic_relation_unit_name(*relations.unit[index])));
    }
    return result.build();
}
auto CanonicalRenderer::build_packed_segment(codegen::PackedSegmentId const segment) -> Expr {
    switch (segment.kind()) {
        case codegen::PackedSegmentKind::field: {
            auto const& fields{store_.packed_fields};
            auto const index{fields.index(segment)};
            auto result{ir_.form(
                "field", {ir_.atom(fields.name[index]), build_type_ref(fields.type[index])})};
            result.field("bits",
                         fields.bits[index].has_value() ? ir_.atom(*fields.bits[index])
                                                        : ir_.atoms.automatic);
            if (fields.kind[index] != codegen::PackedFieldKind::unsigned_integer) {
                result.field("kind", ir_.atom(packed_field_kind_name(fields.kind[index])));
            }
            if (fields.range_helper[index]) {
                result.field("range-helper", ir_.atoms.true_value);
            }
            if (fields.default_value[index].has_value()) {
                result.field("default",
                             ir_.atom(format_packed_integer(*fields.default_value[index])));
            }
            if (fields.minimum_value[index].has_value()) {
                result.field("minimum",
                             ir_.atom(format_packed_integer(*fields.minimum_value[index])));
            }
            if (fields.maximum_value[index].has_value()) {
                result.field("maximum",
                             ir_.atom(format_packed_integer(*fields.maximum_value[index])));
            }
            for (auto const code : items(fields.named_codes[index])) {
                result.add(build_named_code(code));
            }
            if (fields.relationship[index].has_value()) {
                result.add(build_semantic_relation(*fields.relationship[index]));
            }
            return result.build();
        }
        case codegen::PackedSegmentKind::reserved: {
            auto const& reserved_bits{store_.packed_reserved_bits};
            auto const index{reserved_bits.index(segment)};
            return ir_.form("reserved", {ir_.atom(reserved_bits.name[index])})
                .field("bits", reserved_bits.bits[index])
                .build();
        }
        default:
            std::unreachable();
    }
}
auto CanonicalRenderer::build_packed_value(codegen::DeclarationHandle const schema) -> Expr {
    auto const& values{store_.packed_values};
    auto const index{values.index(schema)};
    auto result{ir_.form("packed-value", {ir_.atom(values.name[index])})};
    result.field("storage", build_type_ref(values.storage_type[index]));
    if (values.byte_order[index].has_value()) {
        result.field("byte-order", ir_.atom(packed_byte_order_name(*values.byte_order[index])));
    }
    if (values.bit_order[index].has_value()) {
        result.field("bit-order", ir_.atom(packed_bit_order_name(*values.bit_order[index])));
    }
    if (values.invalid_value[index].has_value()) {
        result.field("invalid-value", *values.invalid_value[index]);
    }
    if (values.export_specifier[index].value != 0) {
        result.field("export-specifier", ir_.atom(values.export_specifier[index]));
    }
    if (values.mutable_value[index]) {
        result.field("mutable", ir_.atoms.true_value);
    }
    for (auto const segment : items(values.segments[index])) {
        result.add(build_packed_segment(segment));
    }
    return result.build();
}
auto CanonicalRenderer::build_named_code(codegen::PackedNamedCodeId const code) -> Expr {
    auto const& codes{store_.packed_named_codes};
    auto const index{codes.index(code)};
    auto result{ir_.form("code", {ir_.atom(codes.name[index])})};
    result.field("value", ir_.atom(format_packed_integer(codes.value[index])));
    if (codes.sentinel[index]) {
        result.field("sentinel", ir_.atoms.true_value);
    }
    return result.build();
}
auto CanonicalRenderer::build_integer_scalar(codegen::DeclarationHandle const schema) -> Expr {
    auto const& scalars{store_.integer_scalars};
    auto const index{scalars.index(schema)};
    auto result{ir_.form("integer-scalar", {ir_.atom(scalars.name[index])})};
    result.field("signed", scalars.signedness[index] ? ir_.atoms.true_value : ir_.atoms.false_value)
        .field("minimum", ir_.atom(format_packed_integer(scalars.minimum_value[index])))
        .field("maximum", ir_.atom(format_packed_integer(scalars.maximum_value[index])))
        .field("bit-width",
               scalars.bit_width[index].has_value() ? ir_.atom(*scalars.bit_width[index])
                                                    : ir_.atoms.automatic);
    if (scalars.cpp_emission[index] != codegen::IntegerScalarCppEmission::none) {
        result.field("cpp-emission",
                     ir_.atom(integer_scalar_cpp_emission_name(scalars.cpp_emission[index])));
    }
    if (scalars.cpp_type[index].has_value()) {
        result.field("cpp-type", build_type_ref(*scalars.cpp_type[index]));
    }
    for (auto const code : items(scalars.named_codes[index])) {
        result.add(build_named_code(code));
    }
    if (scalars.relationship[index].has_value()) {
        result.add(build_semantic_relation(*scalars.relationship[index]));
    }
    return result.build();
}
auto CanonicalRenderer::build_linear_quantized(codegen::DeclarationHandle const schema) -> Expr {
    auto const& values{store_.linear_quantizeds};
    auto const index{values.index(schema)};
    return ir_.form("linear-quantized", {ir_.atom(values.name[index])})
        .field("source", build_type_ref(values.source[index]))
        .field("bits", values.bit_width[index])
        .field("reserved-codes", values.reserved_codes[index])
        .field("clipping", ir_.atom(quantization_clipping_name(values.clipping[index])))
        .build();
}
auto CanonicalRenderer::build_integer_varint(codegen::DeclarationHandle const schema) -> Expr {
    auto const& values{store_.integer_varints};
    auto const index{values.index(schema)};
    return ir_.form("integer-varint", {ir_.atom(values.name[index])})
        .field("source", build_type_ref(values.source[index]))
        .field("encoding", ir_.atom(integer_varint_encoding_name(values.encoding[index])))
        .build();
}
auto CanonicalRenderer::build_fixed_point(codegen::DeclarationHandle const schema) -> Expr {
    auto const& values{store_.fixed_points};
    auto const index{values.index(schema)};
    auto result{ir_.form("fixed-point", {ir_.atom(values.name[index])})};
    result.field("signed", values.signedness[index] ? ir_.atoms.true_value : ir_.atoms.false_value)
        .field("total-bits", values.total_bits[index])
        .field("fractional-bits", values.fractional_bits[index])
        .field("rounding", ir_.atom(fixed_point_rounding_name(values.rounding[index])));
    if (values.minimum_value[index].has_value()) {
        result.field("minimum", ir_.atom(*values.minimum_value[index]));
    }
    if (values.maximum_value[index].has_value()) {
        result.field("maximum", ir_.atom(*values.maximum_value[index]));
    }
    return result.build();
}
auto CanonicalRenderer::build_mini_float(codegen::DeclarationHandle const schema) -> Expr {
    auto const& values{store_.mini_floats};
    auto const index{values.index(schema)};
    return ir_.form("mini-float", {ir_.atom(values.name[index])})
        .field("sign-bits", values.sign_bits[index])
        .field("exponent-bits", values.exponent_bits[index])
        .field("significand-bits", values.significand_bits[index])
        .field("bias", values.exponent_bias[index])
        .build();
}
auto CanonicalRenderer::build_optional_sentinel(codegen::DeclarationHandle const schema) -> Expr {
    auto const& values{store_.optional_sentinels};
    auto const index{values.index(schema)};
    return ir_.form("optional-sentinel", {ir_.atom(values.name[index])})
        .field("source", build_type_ref(values.source[index]))
        .field("sentinel", ir_.atom(values.sentinel[index]))
        .build();
}
auto CanonicalRenderer::build_optional_presence_bit(codegen::DeclarationHandle const schema)
    -> Expr {
    auto const& values{store_.optional_presence_bits};
    auto const index{values.index(schema)};
    return ir_.form("optional-presence-bit", {ir_.atom(values.name[index])})
        .field("source", build_type_ref(values.source[index]))
        .build();
}
auto CanonicalRenderer::build_record(codegen::DeclarationHandle const schema) -> Expr {
    auto const& records{store_.records};
    auto const index{records.index(schema)};
    auto result{ir_.form("record", {ir_.atom(records.name[index])})};
    if (records.export_specifier[index].value != 0) {
        result.field("export-specifier", ir_.atom(records.export_specifier[index]));
    }
    if (records.comparison[index] != codegen::RecordComparison::none) {
        result.field("comparison",
                     ir_.atom(records.comparison[index] == codegen::RecordComparison::equality
                                  ? "equality"
                                  : "three-way"));
    }
    if (records.comparison_noexcept[index]) {
        result.field("comparison-noexcept", ir_.atoms.true_value);
    }
    for (auto const member : items(records.members[index])) {
        result.add(build_aggregate_child(store_.record_members, "member", member));
    }
    for (auto const function : items(records.functions[index])) {
        result.add(build_function(function));
    }
    return result.build();
}
auto CanonicalRenderer::build_union(codegen::DeclarationHandle const schema) -> Expr {
    auto const& unions{store_.unions};
    auto const index{unions.index(schema)};
    auto result{ir_.form("union", {ir_.atom(unions.name[index])})};
    if (unions.export_specifier[index].value != 0) {
        result.field("export-specifier", ir_.atom(unions.export_specifier[index]));
    }
    for (auto const alternative : items(unions.alternatives[index])) {
        result.add(build_aggregate_child(store_.union_alternatives, "alternative", alternative));
    }
    return result.build();
}
auto CanonicalRenderer::build_tagged_union_alternative(
    codegen::TaggedUnionAlternativeId const alternative) -> Expr {
    auto const& alternatives{store_.tagged_union_alternatives};
    auto const index{alternatives.index(alternative)};
    auto result{
        ir_.form("alternative",
                 {ir_.atom(alternatives.name[index]), build_type_ref(alternatives.type[index])})};
    result.field("tag", ir_.atom(alternatives.tag[index]));
    if (alternatives.count[index].has_value()) {
        result.field("count", *alternatives.count[index]);
    }
    return result.build();
}
auto CanonicalRenderer::build_tagged_union(codegen::DeclarationHandle const schema) -> Expr {
    auto const& unions{store_.tagged_unions};
    auto const index{unions.index(schema)};
    auto result{ir_.form("tagged-union", {ir_.atom(unions.name[index])})};
    result.field("discriminant", build_type_ref(unions.discriminant[index]));
    if (unions.export_specifier[index].value != 0) {
        result.field("export-specifier", ir_.atom(unions.export_specifier[index]));
    }
    for (auto const alternative : items(unions.alternatives[index])) {
        result.add(build_tagged_union_alternative(alternative));
    }
    return result.build();
}
auto CanonicalRenderer::build_soa_member(codegen::SoaMemberId const member) -> Expr {
    auto const& dimensions{store_.soa_mask_dimensions};
    auto const& members{store_.soa_members};
    auto const index{members.index(member)};
    auto result{ir_.form("member",
                         {ir_.atom(members.name[index]),
                          ir_.atom(soa_member_kind_name(members.kind[index])),
                          build_type_ref(members.type[index])})};
    if (members.fixed_schema[index].has_value()) {
        result.field("fixed-schema", ir_.atom(*members.fixed_schema[index]));
    }
    if (members.nested_schema[index].has_value()) {
        result.field("nested-schema", ir_.atom(*members.nested_schema[index]));
    }
    if (members.mask_field[index]) {
        result.field("mask-field", ir_.atoms.true_value);
    }
    auto const mask_dimensions{items(members.mask_dimensions[index])};
    if (!mask_dimensions.empty()) {
        std::vector<Expr> values;
        for (auto const dimension : mask_dimensions) {
            auto const dimension_index{dimensions.index(dimension)};
            values.push_back(ir_.list({ir_.atom(dimensions.index_name[dimension_index]),
                                       ir_.string(dimensions.extent[dimension_index])}));
        }
        result.field("mask-dimensions", ir_.list(values));
    }
    if (members.relationship[index].has_value()) {
        result.add(build_semantic_relation(*members.relationship[index]));
    }
    return result.build();
}
auto CanonicalRenderer::build_soa(codegen::DeclarationHandle const schema) -> Expr {
    auto const& soas{store_.soas};
    auto const index{soas.index(schema)};
    auto result{ir_.form("struct", {ir_.atom(soas.name[index])})};
    if (soas.view_name[index].has_value()) {
        result.field("view-name", ir_.atom(*soas.view_name[index]));
    }
    if (soas.const_view_name[index].has_value()) {
        result.field("const-view-name", ir_.atom(*soas.const_view_name[index]));
    }
    auto const operations{items(soas.operations[index])};
    if (!operations.empty()) {
        std::vector<Expr> values;
        for (auto const operation : operations) {
            values.push_back(ir_.atom(storage_operation_name(operation)));
        }
        result.field("operations", ir_.list(values));
    }
    if (soas.export_specifier[index].value != 0) {
        result.field("export-specifier", ir_.atom(soas.export_specifier[index]));
    }
    if (!items(soas.using_declarations[index]).empty()) {
        result.field("using-declarations", string_list(items(soas.using_declarations[index])));
    }
    if (soas.equivalent_type[index].has_value()) {
        result.field("equivalent-type", build_type_ref(*soas.equivalent_type[index]));
    }
    auto const components{items(soas.vector_components[index])};
    if (!components.empty()) {
        result.field("vector-components", atom_list(components));
    }
    if (soas.copy_element_memberwise[index]) {
        result.field("copy-element-memberwise", ir_.atoms.true_value);
    }
    if (soas.layout_only[index]) {
        result.field("layout-only", ir_.atoms.true_value);
    }
    if (soas.storage[index]) {
        result.field("storage", ir_.atom(codegen::soa_storage_name(*soas.storage[index])));
    }
    if (soas.field_mask_name[index].has_value()) {
        result.field("field-mask-name", ir_.atom(*soas.field_mask_name[index]));
    }
    if (soas.field_enum_name[index].has_value()) {
        result.field("field-enum-name", ir_.atom(*soas.field_enum_name[index]));
    }
    for (auto const member : items(soas.members[index])) {
        result.add(build_soa_member(member));
    }
    for (auto const function : items(soas.functions[index])) {
        result.add(build_function(function));
    }
    for (auto const function : items(soas.const_view_functions[index])) {
        result.add(build_function(function, "view-function"));
    }
    for (auto const function : items(soas.mutable_view_functions[index])) {
        result.add(build_function(function, "mutable-view-function"));
    }
    if (soas.array_allocator[index]) {
        result.field("array-allocator", build_type_ref(*soas.array_allocator[index]));
    }
    if (soas.fixed[index].has_value()) {
        auto const& fixed_soas{store_.fixed_soas};
        auto const fixed_index{fixed_soas.index(*soas.fixed[index])};
        auto fixed{ir_.form("fixed", {ir_.atom(fixed_soas.storage_name[fixed_index])})};
        auto const containers{items(fixed_soas.containers[fixed_index])};
        if (!containers.empty()) {
            fixed.field("containers", atom_list(containers));
        }
        result.add(fixed.build());
    }
    if (soas.single_allocation[index].has_value()) {
        result.add(
            ir_.form("single-allocation", {ir_.atom(*soas.single_allocation[index])}).build());
    }
    return result.build();
}
auto CanonicalRenderer::build_vector_soa(codegen::DeclarationHandle const schema) -> Expr {
    auto const& soas{store_.vector_soas};
    auto const index{soas.index(schema)};
    auto result{ir_.form("vector-soa", {ir_.atom(soas.name[index])})};
    result.field("value-type", build_type_ref(soas.value_type[index]))
        .field("components", string_list(items(soas.components[index])));
    if (!items(soas.equivalent_members[index]).empty()) {
        result.field("equivalent-members", string_list(items(soas.equivalent_members[index])));
    }
    if (soas.equivalent_accessors[index]) {
        result.field("equivalent-accessors", ir_.atoms.true_value);
    }
    if (soas.equivalent_constructor[index].has_value()) {
        result.field("equivalent-constructor", ir_.atom(*soas.equivalent_constructor[index]));
    }
    result.field("equivalent-type", build_type_ref(soas.equivalent_type[index]));
    if (soas.export_specifier[index].value != 0) {
        result.field("export-specifier", ir_.atom(soas.export_specifier[index]));
    }
    if (soas.fixed[index].has_value()) {
        auto const& fixed_soas{store_.fixed_soas};
        auto const fixed_index{fixed_soas.index(*soas.fixed[index])};
        auto fixed{ir_.form("fixed", {ir_.atom(fixed_soas.storage_name[fixed_index])})};
        auto const containers{items(fixed_soas.containers[fixed_index])};
        if (!containers.empty()) {
            fixed.field("containers", string_list(containers));
        }
        result.add(fixed.build());
    }
    return result.build();
}
auto CanonicalRenderer::build_homogeneous_layout(codegen::DeclarationHandle const schema) -> Expr {
    auto const& values{store_.homogeneous_values};
    auto const& layouts{store_.homogeneous_layouts};
    auto const index{layouts.index(schema)};
    auto result{ir_.form("layout", {ir_.atom(layouts.name[index])})};
    result.field("components", string_list(items(layouts.components[index])));
    if (!items(layouts.input_members[index]).empty()) {
        result.field("input-members", string_list(items(layouts.input_members[index])));
    }
    if (layouts.export_specifier[index].value != 0) {
        result.field("export-specifier", ir_.atom(layouts.export_specifier[index]));
    }
    for (auto const value : items(layouts.value_types[index])) {
        auto const value_index{values.index(value)};
        auto child{ir_.form(
            "value-type",
            {build_type_ref(values.type[value_index]), ir_.atom(values.suffix[value_index])})};
        if (values.equivalent_type[value_index].has_value()) {
            child.field("equivalent-type", build_type_ref(*values.equivalent_type[value_index]));
        }
        auto const input_types{items(values.input_types[value_index])};
        if (!input_types.empty()) {
            std::vector<Expr> types;
            for (auto const input_type : input_types) {
                types.push_back(build_type_ref(input_type));
            }
            child.field("input-types", ir_.list(types));
        }
        result.add(child.build());
    }
    return result.build();
}
auto CanonicalRenderer::build_static_table(codegen::DeclarationHandle const schema) -> Expr {
    auto const& groups{store_.static_table_groups};
    auto const& columns{store_.static_table_columns};
    auto const& tables{store_.static_tables};
    auto const index{tables.index(schema)};
    auto result{ir_.form("table", {ir_.atom(tables.name[index])})};
    if (tables.export_specifier[index].value != 0) {
        result.field("export-specifier", ir_.atom(tables.export_specifier[index]));
    }
    for (auto const row : items(tables.rows[index])) {
        result.add(
            ir_.form("row",
                     {ir_.atom(store_.static_table_rows.name[store_.static_table_rows.index(row)])})
                .build());
    }
    for (auto const column : items(tables.columns[index])) {
        auto const column_index{columns.index(column)};
        result.add(ir_.form("column",
                            {ir_.atom(columns.name[column_index]),
                             build_type_ref(columns.type[column_index])})
                       .build());
    }
    for (auto const group : items(tables.groups[index])) {
        auto const group_index{groups.index(group)};
        result.add(
            ir_.form("group",
                     {ir_.atom(groups.name[group_index]), build_type_ref(groups.type[group_index])})
                .field("columns", string_list(items(groups.columns[group_index])))
                .build());
    }
    return result.build();
}
auto CanonicalRenderer::build_facade(codegen::DeclarationHandle const schema) -> Expr {
    auto const& methods{store_.facade_methods};
    auto const& facades{store_.facades};
    auto const index{facades.index(schema)};
    auto result{ir_.form("facade",
                         {ir_.atom(facades.name[index]),
                          build_type_ref(facades.target_type[index]),
                          ir_.atom(facades.target_member_name[index])})};
    if (!items(facades.validation_lines[index]).empty()) {
        result.field("validation", string_list(items(facades.validation_lines[index])));
    }
    if (!items(facades.validation_dependencies[index]).empty()) {
        result.field("validation-dependencies",
                     string_list(items(facades.validation_dependencies[index])));
    }
    if (facades.export_specifier[index].value != 0) {
        result.field("export-specifier", ir_.atom(facades.export_specifier[index]));
    }
    if (facades.bind_access[index] != codegen::Access::public_) {
        result.field("bind-access", ir_.atom(access_keyword(facades.bind_access[index])));
    }
    if (facades.method_access[index] != codegen::Access::public_) {
        result.field("method-access", ir_.atom(access_keyword(facades.method_access[index])));
    }
    if (!items(facades.friends[index]).empty()) {
        result.field("friends", string_list(items(facades.friends[index])));
    }
    if (facades.friend_kind[index] != codegen::RecordKind::class_) {
        result.field("friend-kind", ir_.atom(codegen::record_keyword(facades.friend_kind[index])));
    }
    if (facades.definitions_in_source[index]) {
        result.field("definitions-in-source", ir_.atoms.true_value);
    }
    if (facades.reference_target[index]) {
        result.field("target-storage", ir_.atom("reference"));
    }
    for (auto const method : items(facades.methods[index])) {
        auto const method_index{methods.index(method)};
        auto child{ir_.form("method",
                            {ir_.atom(methods.name[method_index]),
                             build_type_ref(methods.return_type[method_index])})};
        if (methods.is_const[method_index]) {
            child.field("const", ir_.atoms.true_value);
        }
        if (methods.is_noexcept[method_index]) {
            child.field("noexcept", ir_.atoms.true_value);
        }
        if (methods.target_name[method_index].has_value()) {
            child.field("target-name", ir_.atom(*methods.target_name[method_index]));
        }
        for (auto const parameter : items(methods.parameters[method_index])) {
            child.add(build_parameter(parameter));
        }
        result.add(child.build());
    }
    return result.build();
}
auto CanonicalRenderer::build_settings_schema(codegen::DeclarationHandle const schema) -> Expr {
    auto const& schemas{store_.settings_schemas};
    auto const index{schemas.index(schema)};
    auto result{ir_.form("settings-schema", {ir_.atom(schemas.name[index])})};
    result.field("target", ir_.atom(schemas.target[index]))
        .field("target-header", ir_.string(schemas.target_header[index]));
    auto const paths{items(schemas.paths[index])};
    auto const members{items(schemas.members[index])};
    auto const count{paths.size()};
    for (std::size_t field_index{}; field_index < count; ++field_index) {
        result.add(
            ir_.form("field", {ir_.string(paths[field_index]), ir_.string(members[field_index])})
                .build());
    }
    return result.build();
}
auto CanonicalRenderer::build_declaration_schema(codegen::DeclarationHandle const declaration)
    -> Expr {
    switch (declaration.kind()) {
        case codegen::DeclarationKind::enumeration:
            return build_enum(declaration);
        case codegen::DeclarationKind::integer_scalar:
            return build_integer_scalar(declaration);
        case codegen::DeclarationKind::linear_quantized:
            return build_linear_quantized(declaration);
        case codegen::DeclarationKind::integer_varint:
            return build_integer_varint(declaration);
        case codegen::DeclarationKind::fixed_point:
            return build_fixed_point(declaration);
        case codegen::DeclarationKind::optional_sentinel:
            return build_optional_sentinel(declaration);
        case codegen::DeclarationKind::optional_presence_bit:
            return build_optional_presence_bit(declaration);
        case codegen::DeclarationKind::mini_float:
            return build_mini_float(declaration);
        case codegen::DeclarationKind::packed_value:
            return build_packed_value(declaration);
        case codegen::DeclarationKind::record:
            return build_record(declaration);
        case codegen::DeclarationKind::union_type:
            return build_union(declaration);
        case codegen::DeclarationKind::tagged_union:
            return build_tagged_union(declaration);
        case codegen::DeclarationKind::soa:
            return build_soa(declaration);
        case codegen::DeclarationKind::vector_soa:
            return build_vector_soa(declaration);
        case codegen::DeclarationKind::homogeneous_layout:
            return build_homogeneous_layout(declaration);
        case codegen::DeclarationKind::static_table:
            return build_static_table(declaration);
        case codegen::DeclarationKind::facade:
            return build_facade(declaration);
        case codegen::DeclarationKind::settings_schema:
            return build_settings_schema(declaration);
        default:
            std::unreachable();
    }
}
auto CanonicalRenderer::build_module(codegen::ModuleHandle const module) -> Expr {
    auto const& allocators{store_.soa_allocator_variants};
    auto const settings{store_.module_settings(module)};
    auto const& settings_store{store_.module_settings_store};
    auto const index{settings_store.index(settings)};
    auto const head{module.kind() == codegen::ModuleKind::normal     ? "module"
                    : module.kind() == codegen::ModuleKind::settings ? "settings-module"
                                                                     : "umbrella-module"};
    auto result{ir_.form(head, {ir_.atom(settings_store.name[index])})};
    result.field("header", ir_.string(settings_store.header[index]));
    if (settings_store.source[index].has_value()) {
        result.field("source", ir_.string(*settings_store.source[index]));
    }
    if (settings_store.header_include[index].has_value()) {
        result.field("header-include", ir_.string(*settings_store.header_include[index]));
    }
    if (settings_store.namespace_name[index].has_value()) {
        result.field("namespace", ir_.atom(*settings_store.namespace_name[index]));
    }
    if (!settings_store.include_order[index].empty()) {
        result.field("include-order", string_list(items(settings_store.include_order[index])));
    }
    if (!settings_store.prelude_lines[index].empty()) {
        result.field("prelude", string_list(items(settings_store.prelude_lines[index])));
    }

    if (module.kind() == codegen::ModuleKind::umbrella) {
        auto const& modules{store_.umbrella_modules};
        result.field("headers", string_list(items(modules.headers[modules.index(module)])));
        return result.build();
    }
    if (module.kind() == codegen::ModuleKind::settings) {
        build_settings_module(result, module);
        return result.build();
    }

    auto const& modules{store_.normal_modules};
    auto const module_index{modules.index(module)};
    if (modules.enum_helper_namespace[module_index].has_value()) {
        result.field("helper-namespace", ir_.atom(*modules.enum_helper_namespace[module_index]));
    }
    if (modules.soa_backend[module_index] == codegen::SoaBackend::standard_library) {
        result.field("backend", ir_.atom("standard-library"));
    }
    for (auto const allocator : items(modules.soa_array_allocators[module_index])) {
        auto const allocator_index{allocators.index(allocator)};
        result.add(ir_.form("array-allocator",
                            {ir_.atom(allocators.prefix[allocator_index]),
                             build_type_ref(allocators.allocator[allocator_index])})
                       .build());
    }
    for (auto const declaration : items(modules.declarations[module_index])) {
        result.add(build_declaration_schema(declaration));
    }
    return result.build();
}
void CanonicalRenderer::build_settings_module(codegen::sexpr::FormBuilder& result,
                                              codegen::ModuleHandle const module) {
    auto const& settings{store_.settings};
    auto const& controls{store_.setting_controls};
    auto const& categories{store_.setting_categories};
    auto const& modules{store_.settings_modules};
    auto const index{modules.index(module)};
    result.field("api-name", ir_.atom(modules.api_name[index]))
        .field("state-name", ir_.atom(modules.state_name[index]));
    if (modules.export_specifier[index].value != 0) {
        result.field("export-specifier", ir_.atom(modules.export_specifier[index]));
    }
    for (auto const category : items(modules.categories[index])) {
        auto const category_index{categories.index(category)};
        result.add(ir_.form("category",
                            {ir_.atom(categories.name[category_index]),
                             ir_.string(categories.label[category_index])})
                       .build());
    }
    for (auto const setting : items(modules.settings_list[index])) {
        auto const row{settings.index(setting)};
        auto const apply{
            settings.apply_mode[row] == codegen::SettingApplyMode::immediate  ? "immediate"
            : settings.apply_mode[row] == codegen::SettingApplyMode::deferred ? "deferred"
                                                                              : "confirm"};
        auto const device{settings.device[row] == codegen::SettingDevice::shared ? "shared"
                          : settings.device[row] == codegen::SettingDevice::keyboard_mouse
                              ? "keyboard-mouse"
                              : "controller"};
        auto child{
            ir_.form("setting", {ir_.atom(settings.name[row]), ir_.string(settings.label[row])})};
        child.field("category", ir_.atom(settings.category[row]))
            .field("value-type", build_type_ref(settings.value_type[row]))
            .field("backend", ir_.atom(settings.backend[row]))
            .field("apply", ir_.atom(apply))
            .field("device", ir_.atom(device));
        if (settings.tooltip[row]) {
            child.field("tooltip", ir_.string(*settings.tooltip[row]));
        }

        auto const control{controls.index(settings.control[row])};
        std::string_view kind;
        switch (controls.kind[control]) {
            case codegen::SettingControlKind::toggle:
                kind = "toggle";
                break;
            case codegen::SettingControlKind::choice:
                kind = "choice";
                break;
            case codegen::SettingControlKind::float_range:
                kind = "float-range";
                break;
            case codegen::SettingControlKind::integer_range:
                kind = "integer-range";
                break;
            case codegen::SettingControlKind::custom:
                kind = "custom";
                break;
        }
        auto control_form{ir_.form("control", {ir_.atom(kind)})};
        if (controls.options_provider[control]) {
            control_form.field("options-provider", ir_.string(*controls.options_provider[control]));
        }
        if (controls.availability_provider[control]) {
            control_form.field("availability-provider",
                               ir_.string(*controls.availability_provider[control]));
        }
        if (controls.minimum[control]) {
            control_form.field("min", ir_.atom(std::format("{:.17g}", *controls.minimum[control])));
        }
        if (controls.maximum[control]) {
            control_form.field("max", ir_.atom(std::format("{:.17g}", *controls.maximum[control])));
        }
        if (controls.step[control]) {
            control_form.field("step", ir_.atom(std::format("{:.17g}", *controls.step[control])));
        }
        if (controls.custom_row[control]) {
            control_form.field("custom-row", ir_.string(*controls.custom_row[control]));
        }
        child.add(control_form.build());
        result.add(child.build());
    }
}

} // namespace lispb::schema::detail
