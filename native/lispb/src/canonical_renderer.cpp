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
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace lispb::schema::detail {

CanonicalRenderer::CanonicalRenderer(codegen::Declarations const& declarations)
    : store_{declarations} {}
auto CanonicalRenderer::text(StringId value) const -> std::string_view {
    return store_.strings().view(value);
}
auto CanonicalRenderer::text(IdentifierId value) const -> std::string_view {
    return store_.strings().view(value);
}
auto CanonicalRenderer::quote(StringId value) -> std::string {
    return quote(text(value));
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
auto CanonicalRenderer::quote(std::string_view const value) -> std::string {
    std::string result{"\""};
    for (auto const character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += character;
                break;
        }
    }
    result += '"';
    return result;
}
auto CanonicalRenderer::render_type_ref(codegen::TypeRefId id) -> std::string {
    auto const type{id};
    auto const name{text(store_.type_refs.name[store_.type_refs.index(type)])};
    auto const needs_quotes{name.empty() || name.starts_with('#') ||
                            (name.starts_with(':') && !name.starts_with("::")) ||
                            std::ranges::any_of(name, [](unsigned char character) {
                                return std::isspace(character) != 0 || character == '(' ||
                                       character == ')' || character == '"' || character == ';';
                            })};
    auto const rendered_name{needs_quotes ? quote(name) : std::string{name}};
    if (text(store_.type_refs.suffix[store_.type_refs.index(type)]).empty() &&
        !store_.type_refs.nested[store_.type_refs.index(type)].has_value()) {
        return rendered_name;
    }
    auto result{std::string{"(type-ref "} + rendered_name};
    if (!text(store_.type_refs.suffix[store_.type_refs.index(type)]).empty()) {
        result += " :suffix " + quote(text(store_.type_refs.suffix[store_.type_refs.index(type)]));
    }
    if (store_.type_refs.nested[store_.type_refs.index(type)].has_value()) {
        result += " :nested " + quote(text(*store_.type_refs.nested[store_.type_refs.index(type)]));
    }
    return result + ')';
}
auto CanonicalRenderer::render_enum_value(lispb::codegen::EnumeratorId const& value)
    -> std::string {
    std::ostringstream output;
    output << "(value " << text(store_.enumerators.name[store_.enumerators.index(value)]);
    if (store_.enumerators.initializer[store_.enumerators.index(value)].has_value()) {
        output << "\n      :value "
               << quote(text(*store_.enumerators.initializer[store_.enumerators.index(value)]));
    }
    if (store_.enumerators.display_name[store_.enumerators.index(value)].has_value()) {
        output << "\n      :display-name "
               << quote(text(*store_.enumerators.display_name[store_.enumerators.index(value)]));
    }
    if (store_.enumerators.hidden[store_.enumerators.index(value)]) {
        output << "\n      :hidden true";
    }
    if (store_.enumerators.serialized_name[store_.enumerators.index(value)].has_value()) {
        output << "\n      :serialized-name "
               << quote(text(*store_.enumerators.serialized_name[store_.enumerators.index(value)]));
    }
    if (store_.enumerators.sentinel[store_.enumerators.index(value)]) {
        output << "\n      :sentinel true";
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_enum_unreal_projection(
    lispb::codegen::EnumUnrealProjectionId const& projection) -> std::string {
    std::ostringstream output;
    output << "(unreal-projection "
           << text(store_.enum_unreal_projections
                       .name[store_.enum_unreal_projections.index(projection)])
           << "\n      :header "
           << quote(text(store_.enum_unreal_projections
                             .header[store_.enum_unreal_projections.index(projection)]))
           << "\n      :header-include "
           << quote(text(store_.enum_unreal_projections
                             .header_include[store_.enum_unreal_projections.index(projection)]))
           << "\n      :conversion-header "
           << quote(text(store_.enum_unreal_projections
                             .conversion_header[store_.enum_unreal_projections.index(projection)]))
           << "\n      :native-header-include "
           << quote(text(
                  store_.enum_unreal_projections
                      .native_header_include[store_.enum_unreal_projections.index(projection)]));
    if (store_.enum_unreal_projections
            .enum_array[store_.enum_unreal_projections.index(projection)]) {
        output << "\n      :enum-array true";
    }
    if (store_.enum_unreal_projections
            .reflection[store_.enum_unreal_projections.index(projection)] !=
        codegen::EnumReflection::uenum) {
        output << "\n      :reflection "
               << codegen::enum_reflection_name(
                      store_.enum_unreal_projections
                          .reflection[store_.enum_unreal_projections.index(projection)]);
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_enum(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(enum " << text(store_.enums.name[store_.enums.index(schema)]);
    if (store_.enums.underlying_type[store_.enums.index(schema)].has_value()) {
        output << ' ' << render_type_ref(*store_.enums.underlying_type[store_.enums.index(schema)]);
    }
    if (store_.enums.bit_width[store_.enums.index(schema)].has_value()) {
        output << "\n    :bit-width "
               << static_cast<unsigned>(*store_.enums.bit_width[store_.enums.index(schema)]);
    }
    if (store_.enums.signedness[store_.enums.index(schema)] != EnumSignedness::unspecified) {
        output << "\n    :signed "
               << (store_.enums.signedness[store_.enums.index(schema)] == EnumSignedness::signed_
                       ? "true"
                       : "false");
    }
    if (store_.enums.reflection[store_.enums.index(schema)] != codegen::EnumReflection::none) {
        output << "\n    :reflection "
               << codegen::enum_reflection_name(
                      store_.enums.reflection[store_.enums.index(schema)]);
    }
    if (store_.enums.enum_array[store_.enums.index(schema)]) {
        output << "\n    :enum-array true";
    }
    if (store_.enums.count[store_.enums.index(schema)].has_value()) {
        output << "\n    :count " << text(*store_.enums.count[store_.enums.index(schema)]);
    }
    if (!items(store_.enums.conversions[store_.enums.index(schema)]).empty()) {
        output << "\n    :conversions (";
        for (std::size_t index{};
             index < items(store_.enums.conversions[store_.enums.index(schema)]).size();
             ++index) {
            output << (index == 0 ? "" : " ")
                   << codegen::enum_conversion_name(
                          items(store_.enums.conversions[store_.enums.index(schema)])[index]);
        }
        output << ')';
    }
    if (store_.enums.export_specifier[store_.enums.index(schema)].value != 0) {
        output << "\n    :export-specifier "
               << text(store_.enums.export_specifier[store_.enums.index(schema)]);
    }
    if (store_.enums.native_api[store_.enums.index(schema)]) {
        output << "\n    :native-api true";
    }
    if (!items(store_.enums.helpers[store_.enums.index(schema)]).empty()) {
        output << "\n    :helpers (";
        for (std::size_t index{};
             index < items(store_.enums.helpers[store_.enums.index(schema)]).size();
             ++index) {
            output << (index == 0 ? "" : " ")
                   << codegen::native_enum_helper_name(
                          items(store_.enums.helpers[store_.enums.index(schema)])[index]);
        }
        output << ')';
    }
    for (auto const& value : items(store_.enums.values[store_.enums.index(schema)])) {
        output << "\n    " << render_enum_value(value);
    }
    if (store_.enums.unreal_projection[store_.enums.index(schema)].has_value()) {
        output << "\n    "
               << render_enum_unreal_projection(
                      *store_.enums.unreal_projection[store_.enums.index(schema)]);
    }
    output << ')';
    return output.str();
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
void CanonicalRenderer::render_function(std::ostringstream& output,
                                        lispb::codegen::FunctionSchemaId const& function,
                                        std::string_view const indent,
                                        std::string_view const kind) {
    output << indent << "(" << kind << " "
           << text(store_.functions.name[store_.functions.index(function)]) << ' '
           << render_type_ref(store_.functions.return_type[store_.functions.index(function)]);
    if (!texts(store_.functions.body_lines[store_.functions.index(function)]).empty()) {
        output << "\n" << indent << "  :body ";
        render_quoted_list(output,
                           texts(store_.functions.body_lines[store_.functions.index(function)]));
    }
    if (!texts(store_.functions.dependencies[store_.functions.index(function)]).empty()) {
        output << "\n" << indent << "  :dependencies ";
        render_quoted_list(output,
                           texts(store_.functions.dependencies[store_.functions.index(function)]));
    }
    if (store_.functions.trailing_return_type[store_.functions.index(function)].has_value()) {
        output << "\n"
               << indent << "  :trailing-return-type "
               << render_type_ref(
                      *store_.functions.trailing_return_type[store_.functions.index(function)]);
    }
    if (store_.functions.is_const[store_.functions.index(function)]) {
        output << "\n" << indent << "  :const true";
    }
    if (store_.functions.is_noexcept[store_.functions.index(function)]) {
        output << "\n" << indent << "  :noexcept true";
    }
    if (store_.functions.is_static[store_.functions.index(function)]) {
        output << "\n" << indent << "  :static true";
    }
    if (store_.functions.is_constexpr[store_.functions.index(function)]) {
        output << "\n" << indent << "  :constexpr true";
    }
    if (store_.functions.is_nodiscard[store_.functions.index(function)]) {
        output << "\n" << indent << "  :nodiscard true";
    }
    if (store_.functions.is_inline[store_.functions.index(function)]) {
        output << "\n" << indent << "  :inline true";
    }
    if (store_.functions.definition_in_source[store_.functions.index(function)]) {
        output << "\n" << indent << "  :definition-in-source true";
    }
    if (store_.functions.template_parameters[store_.functions.index(function)].has_value()) {
        output << "\n"
               << indent << "  :template-parameters "
               << quote(text(
                      *store_.functions.template_parameters[store_.functions.index(function)]));
    }
    if (store_.functions.requires_clause[store_.functions.index(function)].has_value()) {
        output << "\n"
               << indent << "  :requires "
               << quote(text(*store_.functions.requires_clause[store_.functions.index(function)]));
    }
    for (auto const& parameter :
         items(store_.functions.parameters[store_.functions.index(function)])) {
        output << "\n"
               << indent << "  (parameter "
               << text(store_.parameters.name[store_.parameters.index(parameter)]) << ' '
               << render_type_ref(store_.parameters.type[store_.parameters.index(parameter)]);
        if (store_.parameters.default_value[store_.parameters.index(parameter)].has_value()) {
            output << " :default "
                   << quote(*store_.parameters.default_value[store_.parameters.index(parameter)]);
        }
        output << ')';
    }
    output << ')';
}
auto CanonicalRenderer::render_semantic_relation(
    lispb::codegen::SemanticRelationId const& relationship) -> std::string {
    std::ostringstream output;
    output << "(relation "
           << semantic_relation_kind_name(
                  store_.semantic_relations.kind[store_.semantic_relations.index(relationship)])
           << ' '
           << render_type_ref(
                  store_.semantic_relations.target[store_.semantic_relations.index(relationship)]);
    if (store_.semantic_relations.unit[store_.semantic_relations.index(relationship)].has_value()) {
        output
            << " :unit "
            << semantic_relation_unit_name(
                   *store_.semantic_relations.unit[store_.semantic_relations.index(relationship)]);
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_packed_segment(codegen::PackedSegmentId const segment)
    -> std::string {
    std::ostringstream output;
    [&]() {
        auto const value{segment};
        switch (value.kind()) {
            case lispb::codegen::PackedSegmentKind::field: {

                output << "(field "
                       << text(store_.packed_fields.name[store_.packed_fields.index(value)]) << ' '
                       << render_type_ref(
                              store_.packed_fields.type[store_.packed_fields.index(value)])
                       << " :bits ";
                if (store_.packed_fields.bits[store_.packed_fields.index(value)].has_value()) {
                    output << *store_.packed_fields.bits[store_.packed_fields.index(value)];
                } else {
                    output << "auto";
                }
                if (store_.packed_fields.kind[store_.packed_fields.index(value)] !=
                    codegen::PackedFieldKind::unsigned_integer) {
                    output << " :kind "
                           << packed_field_kind_name(
                                  store_.packed_fields.kind[store_.packed_fields.index(value)]);
                }
                if (store_.packed_fields.range_helper[store_.packed_fields.index(value)]) {
                    output << " :range-helper true";
                }
                if (store_.packed_fields.default_value[store_.packed_fields.index(value)]
                        .has_value()) {
                    output << " :default "
                           << format_packed_integer(
                                  *store_.packed_fields
                                       .default_value[store_.packed_fields.index(value)]);
                }
                if (store_.packed_fields.minimum_value[store_.packed_fields.index(value)]
                        .has_value()) {
                    output << " :minimum "
                           << format_packed_integer(
                                  *store_.packed_fields
                                       .minimum_value[store_.packed_fields.index(value)]);
                }
                if (store_.packed_fields.maximum_value[store_.packed_fields.index(value)]
                        .has_value()) {
                    output << " :maximum "
                           << format_packed_integer(
                                  *store_.packed_fields
                                       .maximum_value[store_.packed_fields.index(value)]);
                }
                for (auto const code :
                     items(store_.packed_fields.named_codes[store_.packed_fields.index(value)])) {
                    output << "\n      " << render_named_code(code);
                }
                if (store_.packed_fields.relationship[store_.packed_fields.index(value)]
                        .has_value()) {
                    output << "\n      "
                           << render_semantic_relation(
                                  *store_.packed_fields
                                       .relationship[store_.packed_fields.index(value)]);
                }
                break;
            }
            case lispb::codegen::PackedSegmentKind::reserved: {

                output
                    << "(reserved "
                    << text(store_.packed_reserved_bits
                                .name[store_.packed_reserved_bits.index(value)])
                    << " :bits "
                    << store_.packed_reserved_bits.bits[store_.packed_reserved_bits.index(value)];
                break;
            }
            default:
                std::unreachable();
        }
        output << ')';
    }();
    return output.str();
}
auto CanonicalRenderer::render_packed_value(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(packed-value "
           << text(store_.packed_values.name[store_.packed_values.index(schema)])
           << "\n    :storage "
           << render_type_ref(
                  store_.packed_values.storage_type[store_.packed_values.index(schema)]);
    if (store_.packed_values.byte_order[store_.packed_values.index(schema)].has_value()) {
        output << "\n    :byte-order "
               << packed_byte_order_name(
                      *store_.packed_values.byte_order[store_.packed_values.index(schema)]);
    }
    if (store_.packed_values.bit_order[store_.packed_values.index(schema)].has_value()) {
        output << "\n    :bit-order "
               << packed_bit_order_name(
                      *store_.packed_values.bit_order[store_.packed_values.index(schema)]);
    }
    if (store_.packed_values.invalid_value[store_.packed_values.index(schema)].has_value()) {
        output << "\n    :invalid-value "
               << *store_.packed_values.invalid_value[store_.packed_values.index(schema)];
    }
    if (store_.packed_values.export_specifier[store_.packed_values.index(schema)].value != 0) {
        output << "\n    :export-specifier "
               << text(store_.packed_values.export_specifier[store_.packed_values.index(schema)]);
    }
    if (store_.packed_values.mutable_value[store_.packed_values.index(schema)]) {
        output << "\n    :mutable true";
    }
    for (auto const& segment :
         items(store_.packed_values.segments[store_.packed_values.index(schema)])) {
        output << "\n    " << render_packed_segment(segment);
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_named_code(lispb::codegen::PackedNamedCodeId const& code)
    -> std::string {
    std::ostringstream output;
    output << "(code "
           << text(store_.packed_named_codes.name[store_.packed_named_codes.index(code)])
           << " :value "
           << format_packed_integer(
                  store_.packed_named_codes.value[store_.packed_named_codes.index(code)]);
    if (store_.packed_named_codes.sentinel[store_.packed_named_codes.index(code)]) {
        output << " :sentinel true";
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_integer_scalar(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(integer-scalar "
           << text(store_.integer_scalars.name[store_.integer_scalars.index(schema)])
           << "\n    :signed "
           << (store_.integer_scalars.signedness[store_.integer_scalars.index(schema)] ? "true"
                                                                                       : "false")
           << "\n    :minimum "
           << format_packed_integer(
                  store_.integer_scalars.minimum_value[store_.integer_scalars.index(schema)])
           << "\n    :maximum "
           << format_packed_integer(
                  store_.integer_scalars.maximum_value[store_.integer_scalars.index(schema)])
           << "\n    :bit-width ";
    if (store_.integer_scalars.bit_width[store_.integer_scalars.index(schema)].has_value()) {
        output << *store_.integer_scalars.bit_width[store_.integer_scalars.index(schema)];
    } else {
        output << "auto";
    }
    if (store_.integer_scalars.cpp_emission[store_.integer_scalars.index(schema)] !=
        codegen::IntegerScalarCppEmission::none) {
        output << "\n    :cpp-emission "
               << integer_scalar_cpp_emission_name(
                      store_.integer_scalars.cpp_emission[store_.integer_scalars.index(schema)]);
    }
    if (store_.integer_scalars.cpp_type[store_.integer_scalars.index(schema)].has_value()) {
        output << "\n    :cpp-type "
               << render_type_ref(
                      *store_.integer_scalars.cpp_type[store_.integer_scalars.index(schema)]);
    }
    for (auto const& code :
         items(store_.integer_scalars.named_codes[store_.integer_scalars.index(schema)])) {
        output << "\n    " << render_named_code(code);
    }
    if (store_.integer_scalars.relationship[store_.integer_scalars.index(schema)].has_value()) {
        output << "\n    "
               << render_semantic_relation(
                      *store_.integer_scalars.relationship[store_.integer_scalars.index(schema)]);
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_linear_quantized(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(linear-quantized "
           << text(store_.linear_quantizeds.name[store_.linear_quantizeds.index(schema)])
           << "\n    :source "
           << render_type_ref(
                  store_.linear_quantizeds.source[store_.linear_quantizeds.index(schema)])
           << "\n    :bits "
           << store_.linear_quantizeds.bit_width[store_.linear_quantizeds.index(schema)]
           << "\n    :reserved-codes "
           << store_.linear_quantizeds.reserved_codes[store_.linear_quantizeds.index(schema)]
           << "\n    :clipping "
           << quantization_clipping_name(
                  store_.linear_quantizeds.clipping[store_.linear_quantizeds.index(schema)])
           << ')';
    return output.str();
}
auto CanonicalRenderer::render_integer_varint(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(integer-varint "
           << text(store_.integer_varints.name[store_.integer_varints.index(schema)])
           << "\n    :source "
           << render_type_ref(store_.integer_varints.source[store_.integer_varints.index(schema)])
           << "\n    :encoding "
           << integer_varint_encoding_name(
                  store_.integer_varints.encoding[store_.integer_varints.index(schema)])
           << ')';
    return output.str();
}
auto CanonicalRenderer::render_fixed_point(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(fixed-point " << text(store_.fixed_points.name[store_.fixed_points.index(schema)])
           << "\n    :signed "
           << (store_.fixed_points.signedness[store_.fixed_points.index(schema)] ? "true" : "false")
           << "\n    :total-bits "
           << store_.fixed_points.total_bits[store_.fixed_points.index(schema)]
           << "\n    :fractional-bits "
           << store_.fixed_points.fractional_bits[store_.fixed_points.index(schema)]
           << "\n    :rounding "
           << fixed_point_rounding_name(
                  store_.fixed_points.rounding[store_.fixed_points.index(schema)]);
    if (store_.fixed_points.minimum_value[store_.fixed_points.index(schema)].has_value()) {
        output << "\n    :minimum "
               << text(*store_.fixed_points.minimum_value[store_.fixed_points.index(schema)]);
    }
    if (store_.fixed_points.maximum_value[store_.fixed_points.index(schema)].has_value()) {
        output << "\n    :maximum "
               << text(*store_.fixed_points.maximum_value[store_.fixed_points.index(schema)]);
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_mini_float(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(mini-float " << text(store_.mini_floats.name[store_.mini_floats.index(schema)])
           << "\n    :sign-bits " << store_.mini_floats.sign_bits[store_.mini_floats.index(schema)]
           << "\n    :exponent-bits "
           << store_.mini_floats.exponent_bits[store_.mini_floats.index(schema)]
           << "\n    :significand-bits "
           << store_.mini_floats.significand_bits[store_.mini_floats.index(schema)]
           << "\n    :bias " << store_.mini_floats.exponent_bias[store_.mini_floats.index(schema)]
           << ')';
    return output.str();
}
auto CanonicalRenderer::render_optional_sentinel(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(optional-sentinel "
           << text(store_.optional_sentinels.name[store_.optional_sentinels.index(schema)])
           << "\n    :source "
           << render_type_ref(
                  store_.optional_sentinels.source[store_.optional_sentinels.index(schema)])
           << "\n    :sentinel "
           << text(store_.optional_sentinels.sentinel[store_.optional_sentinels.index(schema)])
           << ')';
    return output.str();
}
auto
    CanonicalRenderer::render_optional_presence_bit(lispb::codegen::DeclarationHandle const& schema)
        -> std::string {
    std::ostringstream output;
    output << "(optional-presence-bit "
           << text(store_.optional_presence_bits.name[store_.optional_presence_bits.index(schema)])
           << "\n    :source "
           << render_type_ref(
                  store_.optional_presence_bits.source[store_.optional_presence_bits.index(schema)])
           << ')';
    return output.str();
}
auto CanonicalRenderer::render_record(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(record " << text(store_.records.name[store_.records.index(schema)]);
    if (store_.records.export_specifier[store_.records.index(schema)].value != 0) {
        output << "\n    :export-specifier "
               << text(store_.records.export_specifier[store_.records.index(schema)]);
    }
    if (store_.records.comparison[store_.records.index(schema)] !=
        codegen::RecordComparison::none) {
        output << "\n    :comparison "
               << (store_.records.comparison[store_.records.index(schema)] ==
                           codegen::RecordComparison::equality
                       ? "equality"
                       : "three-way");
    }
    if (store_.records.comparison_noexcept[store_.records.index(schema)]) {
        output << "\n    :comparison-noexcept true";
    }
    for (auto const& member : items(store_.records.members[store_.records.index(schema)])) {
        output << "\n    " << render_aggregate_child(store_.record_members, "member", member);
    }
    for (auto const& function : items(store_.records.functions[store_.records.index(schema)])) {
        output << '\n';
        render_function(output, function, "    ");
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_union(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(union " << text(store_.unions.name[store_.unions.index(schema)]);
    if (store_.unions.export_specifier[store_.unions.index(schema)].value != 0) {
        output << "\n    :export-specifier "
               << text(store_.unions.export_specifier[store_.unions.index(schema)]);
    }
    for (auto const& alternative : items(store_.unions.alternatives[store_.unions.index(schema)])) {
        output << "\n    "
               << render_aggregate_child(store_.union_alternatives, "alternative", alternative);
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_tagged_union_alternative(
    lispb::codegen::TaggedUnionAlternativeId const& alternative) -> std::string {
    std::ostringstream output;
    output << "(alternative "
           << text(store_.tagged_union_alternatives
                       .name[store_.tagged_union_alternatives.index(alternative)])
           << ' '
           << render_type_ref(store_.tagged_union_alternatives
                                  .type[store_.tagged_union_alternatives.index(alternative)])
           << " :tag "
           << text(store_.tagged_union_alternatives
                       .tag[store_.tagged_union_alternatives.index(alternative)]);
    if (store_.tagged_union_alternatives.count[store_.tagged_union_alternatives.index(alternative)]
            .has_value()) {
        output << " :count "
               << *store_.tagged_union_alternatives
                       .count[store_.tagged_union_alternatives.index(alternative)];
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_tagged_union(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(tagged-union "
           << text(store_.tagged_unions.name[store_.tagged_unions.index(schema)])
           << "\n    :discriminant "
           << render_type_ref(
                  store_.tagged_unions.discriminant[store_.tagged_unions.index(schema)]);
    if (store_.tagged_unions.export_specifier[store_.tagged_unions.index(schema)].value != 0) {
        output << "\n    :export-specifier "
               << text(store_.tagged_unions.export_specifier[store_.tagged_unions.index(schema)]);
    }
    for (auto const& alternative :
         items(store_.tagged_unions.alternatives[store_.tagged_unions.index(schema)])) {
        output << "\n    " << render_tagged_union_alternative(alternative);
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_soa_member(lispb::codegen::SoaMemberId const& member)
    -> std::string {
    std::ostringstream output;
    output << "(member " << text(store_.soa_members.name[store_.soa_members.index(member)]) << ' '
           << soa_member_kind_name(store_.soa_members.kind[store_.soa_members.index(member)]) << ' '
           << render_type_ref(store_.soa_members.type[store_.soa_members.index(member)]);
    if (store_.soa_members.fixed_schema[store_.soa_members.index(member)].has_value()) {
        output << "\n      :fixed-schema "
               << text(*store_.soa_members.fixed_schema[store_.soa_members.index(member)]);
    }
    if (store_.soa_members.nested_schema[store_.soa_members.index(member)].has_value()) {
        output << "\n      :nested-schema "
               << text(*store_.soa_members.nested_schema[store_.soa_members.index(member)]);
    }
    if (store_.soa_members.mask_field[store_.soa_members.index(member)]) {
        output << "\n      :mask-field true";
    }
    if (!items(store_.soa_members.mask_dimensions[store_.soa_members.index(member)]).empty()) {
        output << "\n      :mask-dimensions (";
        for (std::size_t index{};
             index <
             items(store_.soa_members.mask_dimensions[store_.soa_members.index(member)]).size();
             ++index) {
            auto const& dimension{
                items(store_.soa_members.mask_dimensions[store_.soa_members.index(member)])[index]};
            output << (index == 0 ? "" : " ") << '('
                   << text(store_.soa_mask_dimensions
                               .index_name[store_.soa_mask_dimensions.index(dimension)])
                   << ' '
                   << quote(store_.soa_mask_dimensions
                                .extent[store_.soa_mask_dimensions.index(dimension)])
                   << ')';
        }
        output << ')';
    }
    if (store_.soa_members.relationship[store_.soa_members.index(member)].has_value()) {
        output << "\n      "
               << render_semantic_relation(
                      *store_.soa_members.relationship[store_.soa_members.index(member)]);
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_soa(lispb::codegen::DeclarationHandle const& schema) -> std::string {
    std::ostringstream output;
    output << "(struct " << text(store_.soas.name[store_.soas.index(schema)]);
    if (store_.soas.view_name[store_.soas.index(schema)].has_value()) {
        output << "\n    :view-name " << text(*store_.soas.view_name[store_.soas.index(schema)]);
    }
    if (store_.soas.const_view_name[store_.soas.index(schema)].has_value()) {
        output << "\n    :const-view-name "
               << text(*store_.soas.const_view_name[store_.soas.index(schema)]);
    }
    if (!items(store_.soas.operations[store_.soas.index(schema)]).empty()) {
        output << "\n    :operations (";
        for (std::size_t index{};
             index < items(store_.soas.operations[store_.soas.index(schema)]).size();
             ++index) {
            output << (index == 0 ? "" : " ")
                   << storage_operation_name(
                          items(store_.soas.operations[store_.soas.index(schema)])[index]);
        }
        output << ')';
    }
    if (store_.soas.export_specifier[store_.soas.index(schema)].value != 0) {
        output << "\n    :export-specifier "
               << text(store_.soas.export_specifier[store_.soas.index(schema)]);
    }
    if (!texts(store_.soas.using_declarations[store_.soas.index(schema)]).empty()) {
        output << "\n    :using-declarations ";
        render_quoted_list(output,
                           texts(store_.soas.using_declarations[store_.soas.index(schema)]));
    }
    if (store_.soas.equivalent_type[store_.soas.index(schema)].has_value()) {
        output << "\n    :equivalent-type "
               << render_type_ref(*store_.soas.equivalent_type[store_.soas.index(schema)]);
    }
    if (!texts(store_.soas.vector_components[store_.soas.index(schema)]).empty()) {
        output << "\n    :vector-components (";
        for (std::size_t index{};
             index < texts(store_.soas.vector_components[store_.soas.index(schema)]).size();
             ++index) {
            if (index != 0) {
                output << ' ';
            }
            output << texts(store_.soas.vector_components[store_.soas.index(schema)])[index];
        }
        output << ')';
    }
    if (store_.soas.copy_element_memberwise[store_.soas.index(schema)]) {
        output << "\n    :copy-element-memberwise true";
    }
    if (store_.soas.layout_only[store_.soas.index(schema)]) {
        output << "\n    :layout-only true";
    }
    if (store_.soas.storage[store_.soas.index(schema)]) {
        output << "\n    :storage "
               << codegen::soa_storage_name(*store_.soas.storage[store_.soas.index(schema)]);
    }
    if (store_.soas.field_mask_name[store_.soas.index(schema)].has_value()) {
        output << "\n    :field-mask-name "
               << text(*store_.soas.field_mask_name[store_.soas.index(schema)]);
    }
    if (store_.soas.field_enum_name[store_.soas.index(schema)].has_value()) {
        output << "\n    :field-enum-name "
               << text(*store_.soas.field_enum_name[store_.soas.index(schema)]);
    }
    for (auto const& member : items(store_.soas.members[store_.soas.index(schema)])) {
        output << "\n    " << render_soa_member(member);
    }
    for (auto const& function : items(store_.soas.functions[store_.soas.index(schema)])) {
        output << '\n';
        render_function(output, function, "    ");
    }
    for (auto const& function :
         items(store_.soas.const_view_functions[store_.soas.index(schema)])) {
        output << '\n';
        render_function(output, function, "    ", "view-function");
    }
    for (auto const& function :
         items(store_.soas.mutable_view_functions[store_.soas.index(schema)])) {
        output << '\n';
        render_function(output, function, "    ", "mutable-view-function");
    }
    if (store_.soas.array_allocator[store_.soas.index(schema)]) {
        output << "\n    :array-allocator "
               << render_type_ref(*store_.soas.array_allocator[store_.soas.index(schema)]);
    }
    if (store_.soas.fixed[store_.soas.index(schema)].has_value()) {
        output << "\n    (fixed "
               << text(store_.fixed_soas.storage_name[store_.fixed_soas.index(
                      *store_.soas.fixed[store_.soas.index(schema)])]);
        if (!texts(store_.fixed_soas.containers[store_.fixed_soas.index(
                       *store_.soas.fixed[store_.soas.index(schema)])])
                 .empty()) {
            output << " :containers (";
            for (std::size_t index{};
                 index < texts(store_.fixed_soas.containers[store_.fixed_soas.index(
                                   *store_.soas.fixed[store_.soas.index(schema)])])
                             .size();
                 ++index) {
                output << (index == 0 ? "" : " ")
                       << texts(store_.fixed_soas.containers[store_.fixed_soas.index(
                              *store_.soas.fixed[store_.soas.index(schema)])])[index];
            }
            output << ')';
        }
        output << ')';
    }
    if (store_.soas.single_allocation[store_.soas.index(schema)].has_value()) {
        output << "\n    (single-allocation "
               << text(*store_.soas.single_allocation[store_.soas.index(schema)]);
        output << ')';
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_vector_soa(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(vector-soa " << text(store_.vector_soas.name[store_.vector_soas.index(schema)])
           << "\n    :value-type "
           << render_type_ref(store_.vector_soas.value_type[store_.vector_soas.index(schema)])
           << "\n    :components ";
    render_quoted_list(output,
                       texts(store_.vector_soas.components[store_.vector_soas.index(schema)]));
    if (!texts(store_.vector_soas.equivalent_members[store_.vector_soas.index(schema)]).empty()) {
        output << "\n    :equivalent-members ";
        render_quoted_list(
            output, texts(store_.vector_soas.equivalent_members[store_.vector_soas.index(schema)]));
    }
    if (store_.vector_soas.equivalent_accessors[store_.vector_soas.index(schema)]) {
        output << "\n    :equivalent-accessors true";
    }
    if (store_.vector_soas.equivalent_constructor[store_.vector_soas.index(schema)].has_value()) {
        output << "\n    :equivalent-constructor "
               << text(
                      *store_.vector_soas.equivalent_constructor[store_.vector_soas.index(schema)]);
    }
    output << "\n    :equivalent-type "
           << render_type_ref(store_.vector_soas.equivalent_type[store_.vector_soas.index(schema)]);
    if (store_.vector_soas.export_specifier[store_.vector_soas.index(schema)].value != 0) {
        output << "\n    :export-specifier "
               << text(store_.vector_soas.export_specifier[store_.vector_soas.index(schema)]);
    }
    if (store_.vector_soas.fixed[store_.vector_soas.index(schema)].has_value()) {
        output << "\n    (fixed "
               << text(store_.fixed_soas.storage_name[store_.fixed_soas.index(
                      *store_.vector_soas.fixed[store_.vector_soas.index(schema)])]);
        if (!texts(store_.fixed_soas.containers[store_.fixed_soas.index(
                       *store_.vector_soas.fixed[store_.vector_soas.index(schema)])])
                 .empty()) {
            output << " :containers ";
            render_quoted_list(output,
                               texts(store_.fixed_soas.containers[store_.fixed_soas.index(
                                   *store_.vector_soas.fixed[store_.vector_soas.index(schema)])]));
        }
        output << ')';
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_homogeneous_layout(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(layout "
           << text(store_.homogeneous_layouts.name[store_.homogeneous_layouts.index(schema)])
           << "\n    :components ";
    render_quoted_list(
        output,
        texts(store_.homogeneous_layouts.components[store_.homogeneous_layouts.index(schema)]));
    if (!texts(store_.homogeneous_layouts.input_members[store_.homogeneous_layouts.index(schema)])
             .empty()) {
        output << "\n    :input-members ";
        render_quoted_list(output,
                           texts(store_.homogeneous_layouts
                                     .input_members[store_.homogeneous_layouts.index(schema)]));
    }
    if (store_.homogeneous_layouts.export_specifier[store_.homogeneous_layouts.index(schema)]
            .value != 0) {
        output << "\n    :export-specifier "
               << text(store_.homogeneous_layouts
                           .export_specifier[store_.homogeneous_layouts.index(schema)]);
    }
    for (auto const& value :
         items(store_.homogeneous_layouts.value_types[store_.homogeneous_layouts.index(schema)])) {
        output << "\n    (value-type "
               << render_type_ref(
                      store_.homogeneous_values.type[store_.homogeneous_values.index(value)])
               << ' '
               << text(store_.homogeneous_values.suffix[store_.homogeneous_values.index(value)]);
        if (store_.homogeneous_values.equivalent_type[store_.homogeneous_values.index(value)]
                .has_value()) {
            output << "\n      :equivalent-type "
                   << render_type_ref(
                          *store_.homogeneous_values
                               .equivalent_type[store_.homogeneous_values.index(value)]);
        }
        if (!store_.homogeneous_values.input_types[store_.homogeneous_values.index(value)]
                 .empty()) {
            output << "\n      :input-types (";
            for (std::size_t index{};
                 index <
                 store_.homogeneous_values.input_types[store_.homogeneous_values.index(value)]
                     .size();
                 ++index) {
                output << (index == 0 ? "" : " ")
                       << render_type_ref(items(
                              store_.homogeneous_values
                                  .input_types[store_.homogeneous_values.index(value)])[index]);
            }
            output << ')';
        }
        output << ')';
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_static_table(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(table " << text(store_.static_tables.name[store_.static_tables.index(schema)]);
    if (store_.static_tables.export_specifier[store_.static_tables.index(schema)].value != 0) {
        output << "\n    :export-specifier "
               << text(store_.static_tables.export_specifier[store_.static_tables.index(schema)]);
    }
    for (auto const& row : items(store_.static_tables.rows[store_.static_tables.index(schema)])) {
        output << "\n    (row "
               << text(store_.static_table_rows.name[store_.static_table_rows.index(row)]) << ')';
    }
    for (auto const& column :
         items(store_.static_tables.columns[store_.static_tables.index(schema)])) {
        output << "\n    (column "
               << text(store_.static_table_columns.name[store_.static_table_columns.index(column)])
               << ' '
               << render_type_ref(
                      store_.static_table_columns.type[store_.static_table_columns.index(column)])
               << ')';
    }
    for (auto const& group :
         items(store_.static_tables.groups[store_.static_tables.index(schema)])) {
        output << "\n    (group "
               << text(store_.static_table_groups.name[store_.static_table_groups.index(group)])
               << ' '
               << render_type_ref(
                      store_.static_table_groups.type[store_.static_table_groups.index(group)])
               << " :columns ";
        render_quoted_list(
            output,
            texts(store_.static_table_groups.columns[store_.static_table_groups.index(group)]));
        output << ')';
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_facade(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    std::ostringstream output;
    output << "(facade " << text(store_.facades.name[store_.facades.index(schema)]) << ' '
           << render_type_ref(store_.facades.target_type[store_.facades.index(schema)]) << ' '
           << text(store_.facades.target_member_name[store_.facades.index(schema)]);
    if (!texts(store_.facades.validation_lines[store_.facades.index(schema)]).empty()) {
        output << "\n    :validation ";
        render_quoted_list(output,
                           texts(store_.facades.validation_lines[store_.facades.index(schema)]));
    }
    if (!texts(store_.facades.validation_dependencies[store_.facades.index(schema)]).empty()) {
        output << "\n    :validation-dependencies ";
        render_quoted_list(
            output, texts(store_.facades.validation_dependencies[store_.facades.index(schema)]));
    }
    if (store_.facades.export_specifier[store_.facades.index(schema)].value != 0) {
        output << "\n    :export-specifier "
               << text(store_.facades.export_specifier[store_.facades.index(schema)]);
    }
    if (access_keyword(store_.facades.bind_access[store_.facades.index(schema)]) != "public") {
        output << "\n    :bind-access "
               << access_keyword(store_.facades.bind_access[store_.facades.index(schema)]);
    }
    if (access_keyword(store_.facades.method_access[store_.facades.index(schema)]) != "public") {
        output << "\n    :method-access "
               << access_keyword(store_.facades.method_access[store_.facades.index(schema)]);
    }
    if (!texts(store_.facades.friends[store_.facades.index(schema)]).empty()) {
        output << "\n    :friends ";
        render_quoted_list(output, texts(store_.facades.friends[store_.facades.index(schema)]));
    }
    if (codegen::record_keyword(store_.facades.friend_kind[store_.facades.index(schema)]) !=
        "class") {
        output << "\n    :friend-kind "
               << codegen::record_keyword(store_.facades.friend_kind[store_.facades.index(schema)]);
    }
    if (store_.facades.definitions_in_source[store_.facades.index(schema)]) {
        output << "\n    :definitions-in-source true";
    }
    if (store_.facades.reference_target[store_.facades.index(schema)]) {
        output << "\n    :target-storage reference";
    }
    for (auto const& method : items(store_.facades.methods[store_.facades.index(schema)])) {
        output << "\n    (method "
               << text(store_.facade_methods.name[store_.facade_methods.index(method)]) << ' '
               << render_type_ref(
                      store_.facade_methods.return_type[store_.facade_methods.index(method)]);
        if (store_.facade_methods.is_const[store_.facade_methods.index(method)]) {
            output << "\n      :const true";
        }
        if (store_.facade_methods.is_noexcept[store_.facade_methods.index(method)]) {
            output << "\n      :noexcept true";
        }
        if (store_.facade_methods.target_name[store_.facade_methods.index(method)].has_value()) {
            output << "\n      :target-name "
                   << text(*store_.facade_methods.target_name[store_.facade_methods.index(method)]);
        }
        for (auto const& parameter :
             items(store_.facade_methods.parameters[store_.facade_methods.index(method)])) {
            output << "\n      (parameter "
                   << text(store_.parameters.name[store_.parameters.index(parameter)]) << ' '
                   << render_type_ref(store_.parameters.type[store_.parameters.index(parameter)]);
            if (store_.parameters.default_value[store_.parameters.index(parameter)].has_value()) {
                output << " :default "
                       << quote(
                              *store_.parameters.default_value[store_.parameters.index(parameter)]);
            }
            output << ')';
        }
        output << ')';
    }
    output << ')';
    return output.str();
}
auto CanonicalRenderer::render_settings_schema(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& schemas{store_.settings_schemas};
    auto const row{schemas.index(schema)};
    std::ostringstream output;
    output << "(settings-schema " << text(schemas.name[row]) << "\n    :target "
           << text(schemas.target[row]) << "\n    :target-header "
           << quote(text(schemas.target_header[row]));
    auto const paths{items(schemas.paths[row])};
    auto const members{items(schemas.members[row])};
    auto const count{paths.size()};
    for (std::size_t index{}; index < count; ++index) {
        output << "\n    (field " << quote(text(paths[index])) << ' ' << quote(text(members[index]))
               << ')';
    }
    output << ')';
    return output.str();
}

auto CanonicalRenderer::render_declaration_schema(codegen::DeclarationHandle const declaration)
    -> std::string {
    return [&]() -> std::string {
        auto const value{declaration};
        switch (value.kind()) {
            case lispb::codegen::DeclarationKind::enumeration: {

                return render_enum(value);
            }
            case lispb::codegen::DeclarationKind::integer_scalar: {

                return render_integer_scalar(value);
            }
            case lispb::codegen::DeclarationKind::linear_quantized: {

                return render_linear_quantized(value);
            }
            case lispb::codegen::DeclarationKind::integer_varint: {

                return render_integer_varint(value);
            }
            case lispb::codegen::DeclarationKind::fixed_point: {

                return render_fixed_point(value);
            }
            case lispb::codegen::DeclarationKind::optional_sentinel: {

                return render_optional_sentinel(value);
            }
            case lispb::codegen::DeclarationKind::optional_presence_bit: {

                return render_optional_presence_bit(value);
            }
            case lispb::codegen::DeclarationKind::mini_float: {

                return render_mini_float(value);
            }
            case lispb::codegen::DeclarationKind::packed_value: {

                return render_packed_value(value);
            }
            case lispb::codegen::DeclarationKind::record: {

                return render_record(value);
            }
            case lispb::codegen::DeclarationKind::union_type: {

                return render_union(value);
            }
            case lispb::codegen::DeclarationKind::tagged_union: {

                return render_tagged_union(value);
            }
            case lispb::codegen::DeclarationKind::soa: {

                return render_soa(value);
            }
            case lispb::codegen::DeclarationKind::vector_soa: {

                return render_vector_soa(value);
            }
            case lispb::codegen::DeclarationKind::homogeneous_layout: {

                return render_homogeneous_layout(value);
            }
            case lispb::codegen::DeclarationKind::static_table: {

                return render_static_table(value);
            }
            case lispb::codegen::DeclarationKind::facade: {

                return render_facade(value);
            }
            case lispb::codegen::DeclarationKind::settings_schema: {
                return render_settings_schema(value);
            }
            default:
                std::unreachable();
        }
    }();
}
auto CanonicalRenderer::render_module(codegen::ModuleHandle const module) -> std::string {
    auto const settings{store_.module_settings(module)};
    auto const head{module.kind() == codegen::ModuleKind::normal     ? "module"
                    : module.kind() == codegen::ModuleKind::settings ? "settings-module"
                                                                     : "umbrella-module"};
    std::ostringstream output;
    output << '(' << head << ' '
           << text(store_.module_settings_store.name[store_.module_settings_store.index(settings)])
           << "\n  :header "
           << quote(text(store_.module_settings_store
                             .header[store_.module_settings_store.index(settings)]));
    if (store_.module_settings_store.source[store_.module_settings_store.index(settings)]
            .has_value()) {
        output << "\n  :source "
               << quote(text(*store_.module_settings_store
                                  .source[store_.module_settings_store.index(settings)]));
    }
    if (store_.module_settings_store.header_include[store_.module_settings_store.index(settings)]
            .has_value()) {
        output << "\n  :header-include "
               << quote(*store_.module_settings_store
                             .header_include[store_.module_settings_store.index(settings)]);
    }
    if (store_.module_settings_store.namespace_name[store_.module_settings_store.index(settings)]
            .has_value()) {
        output << "\n  :namespace "
               << text(*store_.module_settings_store
                            .namespace_name[store_.module_settings_store.index(settings)]);
    }
    if (!store_.module_settings_store.include_order[store_.module_settings_store.index(settings)]
             .empty()) {
        output << "\n  :include-order ";
        render_quoted_list(output,
                           texts(store_.module_settings_store
                                     .include_order[store_.module_settings_store.index(settings)]));
    }
    if (!store_.module_settings_store.prelude_lines[store_.module_settings_store.index(settings)]
             .empty()) {
        output << "\n  :prelude ";
        render_quoted_list(output,
                           texts(store_.module_settings_store
                                     .prelude_lines[store_.module_settings_store.index(settings)]));
    }

    if (module.kind() == codegen::ModuleKind::umbrella) {
        output << "\n  :headers ";
        render_quoted_list(
            output, texts(store_.umbrella_modules.headers[store_.umbrella_modules.index(module)]));
        output << ')';
        return output.str();
    }
    if (module.kind() == codegen::ModuleKind::settings) {
        render_settings_module(output, module);
        output << ')';
        return output.str();
    }
    if (store_.normal_modules.enum_helper_namespace[store_.normal_modules.index(module)]
            .has_value()) {
        output << "\n  :helper-namespace "
               << text(*store_.normal_modules
                            .enum_helper_namespace[store_.normal_modules.index(module)]);
    }
    if (store_.normal_modules.soa_backend[store_.normal_modules.index(module)] ==
        codegen::SoaBackend::standard_library) {
        output << "\n  :backend standard-library";
    }
    for (auto const& allocator :
         items(store_.normal_modules.soa_array_allocators[store_.normal_modules.index(module)])) {
        output << "\n  (array-allocator "
               << text(store_.soa_allocator_variants
                           .prefix[store_.soa_allocator_variants.index(allocator)])
               << ' '
               << render_type_ref(store_.soa_allocator_variants
                                      .allocator[store_.soa_allocator_variants.index(allocator)])
               << ')';
    }
    for (auto const& declaration :
         items(store_.normal_modules.declarations[store_.normal_modules.index(module)])) {
        output << "\n  " << render_declaration_schema(declaration);
    }
    output << ')';
    return output.str();
}
void CanonicalRenderer::render_settings_module(std::ostringstream& output,
                                               codegen::ModuleHandle const module) {
    output.precision(std::numeric_limits<double>::max_digits10);
    auto const& modules{store_.settings_modules};
    auto const index{modules.index(module)};
    output << "\n  :api-name " << text(modules.api_name[index]) << "\n  :state-name "
           << text(modules.state_name[index]);
    if (modules.export_specifier[index].value != 0) {
        output << "\n  :export-specifier " << text(modules.export_specifier[index]);
    }
    for (auto const category : items(modules.categories[index])) {
        auto const category_index{store_.setting_categories.index(category)};
        output << "\n  (category " << text(store_.setting_categories.name[category_index]) << ' '
               << quote(store_.setting_categories.label[category_index]) << ')';
    }
    for (auto const setting : items(modules.settings_list[index])) {
        auto const& settings{store_.settings};
        auto const row{settings.index(setting)};
        auto const apply{
            settings.apply_mode[row] == codegen::SettingApplyMode::immediate  ? "immediate"
            : settings.apply_mode[row] == codegen::SettingApplyMode::deferred ? "deferred"
                                                                              : "confirm"};
        auto const device{settings.device[row] == codegen::SettingDevice::shared ? "shared"
                          : settings.device[row] == codegen::SettingDevice::keyboard_mouse
                              ? "keyboard-mouse"
                              : "controller"};
        output << "\n  (setting " << text(settings.name[row]) << ' ' << quote(settings.label[row])
               << "\n    :category " << text(settings.category[row]) << "\n    :value-type "
               << render_type_ref(settings.value_type[row]) << "\n    :backend "
               << text(settings.backend[row]) << "\n    :apply " << apply << "\n    :device "
               << device;
        if (settings.tooltip[row]) {
            output << "\n    :tooltip " << quote(*settings.tooltip[row]);
        }
        auto const& controls{store_.setting_controls};
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
        output << "\n    (control " << kind;
        if (controls.options_provider[control]) {
            output << " :options-provider " << quote(*controls.options_provider[control]);
        }
        if (controls.availability_provider[control]) {
            output << " :availability-provider " << quote(*controls.availability_provider[control]);
        }
        if (controls.minimum[control]) {
            output << " :min " << *controls.minimum[control];
        }
        if (controls.maximum[control]) {
            output << " :max " << *controls.maximum[control];
        }
        if (controls.step[control]) {
            output << " :step " << *controls.step[control];
        }
        if (controls.custom_row[control]) {
            output << " :custom-row " << quote(*controls.custom_row[control]);
        }
        output << "))";
    }
}

} // namespace lispb::schema::detail
