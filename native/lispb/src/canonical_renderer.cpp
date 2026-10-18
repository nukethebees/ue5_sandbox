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
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
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
    auto const& types{store_.type_refs};
    auto const type_index{types.index(id)};
    auto const name{text(types.name[type_index])};
    auto const needs_quotes{name.empty() || name.starts_with('#') ||
                            (name.starts_with(':') && !name.starts_with("::")) ||
                            std::ranges::any_of(name, [](unsigned char character) {
                                return std::isspace(character) != 0 || character == '(' ||
                                       character == ')' || character == '"' || character == ';';
                            })};
    auto const rendered_name{needs_quotes ? quote(name) : std::string{name}};
    if (text(types.suffix[type_index]).empty() && !types.nested[type_index].has_value()) {
        return rendered_name;
    }
    auto result{std::string{"(type-ref "} + rendered_name};
    if (!text(types.suffix[type_index]).empty()) {
        result += " :suffix " + quote(text(types.suffix[type_index]));
    }
    if (types.nested[type_index].has_value()) {
        result += " :nested " + quote(text(*types.nested[type_index]));
    }
    return result + ')';
}
auto CanonicalRenderer::render_enum_value(lispb::codegen::EnumeratorId const& value)
    -> std::string {
    auto const& enumerators{store_.enumerators};
    auto const value_index{enumerators.index(value)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(value {}", text(enumerators.name[value_index]));
    if (enumerators.initializer[value_index].has_value()) {
        write("\n      :value {}", quote(text(*enumerators.initializer[value_index])));
    }
    if (enumerators.display_name[value_index].has_value()) {
        write("\n      :display-name {}", quote(text(*enumerators.display_name[value_index])));
    }
    if (enumerators.hidden[value_index]) {
        output += "\n      :hidden true";
    }
    if (enumerators.serialized_name[value_index].has_value()) {
        write("\n      :serialized-name {}",
              quote(text(*enumerators.serialized_name[value_index])));
    }
    if (enumerators.sentinel[value_index]) {
        output += "\n      :sentinel true";
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_enum_unreal_projection(
    lispb::codegen::EnumUnrealProjectionId const& projection) -> std::string {
    auto const& enum_unreal_projections{store_.enum_unreal_projections};
    auto const projection_index{enum_unreal_projections.index(projection)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(unreal-projection {}\n      :header {}\n      :header-include {}\n      "
          ":conversion-header {}\n      :native-header-include {}",
          text(enum_unreal_projections.name[projection_index]),
          quote(text(enum_unreal_projections.header[projection_index])),
          quote(text(enum_unreal_projections.header_include[projection_index])),
          quote(text(enum_unreal_projections.conversion_header[projection_index])),
          quote(text(enum_unreal_projections.native_header_include[projection_index])));
    if (enum_unreal_projections.enum_array[projection_index]) {
        output += "\n      :enum-array true";
    }
    if (enum_unreal_projections.reflection[projection_index] != codegen::EnumReflection::uenum) {
        write("\n      :reflection {}",
              codegen::enum_reflection_name(enum_unreal_projections.reflection[projection_index]));
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_enum(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& enums{store_.enums};
    auto const schema_index{enums.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(enum {}", text(enums.name[schema_index]));
    if (enums.underlying_type[schema_index].has_value()) {
        write(" {}", render_type_ref(*enums.underlying_type[schema_index]));
    }
    if (enums.bit_width[schema_index].has_value()) {
        write("\n    :bit-width {}", static_cast<unsigned>(*enums.bit_width[schema_index]));
    }
    if (enums.signedness[schema_index] != EnumSignedness::unspecified) {
        write("\n    :signed {}",
              (enums.signedness[schema_index] == EnumSignedness::signed_ ? "true" : "false"));
    }
    if (enums.reflection[schema_index] != codegen::EnumReflection::none) {
        write("\n    :reflection {}",
              codegen::enum_reflection_name(enums.reflection[schema_index]));
    }
    if (enums.enum_array[schema_index]) {
        output += "\n    :enum-array true";
    }
    if (enums.count[schema_index].has_value()) {
        write("\n    :count {}", text(*enums.count[schema_index]));
    }
    auto const conversions{items(enums.conversions[schema_index])};
    if (!conversions.empty()) {
        auto const conversions_count{conversions.size()};
        output += "\n    :conversions (";
        for (std::size_t index{}; index < conversions_count; ++index) {
            write(
                "{}{}", (index == 0 ? "" : " "), codegen::enum_conversion_name(conversions[index]));
        }
        output += ')';
    }
    if (enums.export_specifier[schema_index].value != 0) {
        write("\n    :export-specifier {}", text(enums.export_specifier[schema_index]));
    }
    if (enums.native_api[schema_index]) {
        output += "\n    :native-api true";
    }
    auto const helpers{items(enums.helpers[schema_index])};
    if (!helpers.empty()) {
        auto const helpers_count{helpers.size()};
        output += "\n    :helpers (";
        for (std::size_t index{}; index < helpers_count; ++index) {
            write(
                "{}{}", (index == 0 ? "" : " "), codegen::native_enum_helper_name(helpers[index]));
        }
        output += ')';
    }
    for (auto const& value : items(enums.values[schema_index])) {
        write("\n    {}", render_enum_value(value));
    }
    if (enums.unreal_projection[schema_index].has_value()) {
        write("\n    {}", render_enum_unreal_projection(*enums.unreal_projection[schema_index]));
    }
    output += ')';
    return output;
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
void CanonicalRenderer::render_function(std::string& output,
                                        lispb::codegen::FunctionSchemaId const& function,
                                        std::string_view const indent,
                                        std::string_view const kind) {
    auto const& parameters{store_.parameters};
    auto const& functions{store_.functions};
    auto const function_index{functions.index(function)};

    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("{}({} {} {}",
          indent,
          kind,
          text(functions.name[function_index]),
          render_type_ref(functions.return_type[function_index]));
    if (!texts(functions.body_lines[function_index]).empty()) {
        write("\n{}  :body ", indent);
        render_quoted_list(output, texts(functions.body_lines[function_index]));
    }
    if (!texts(functions.dependencies[function_index]).empty()) {
        write("\n{}  :dependencies ", indent);
        render_quoted_list(output, texts(functions.dependencies[function_index]));
    }
    if (functions.trailing_return_type[function_index].has_value()) {
        write("\n{}  :trailing-return-type {}",
              indent,
              render_type_ref(*functions.trailing_return_type[function_index]));
    }
    if (functions.is_const[function_index]) {
        write("\n{}  :const true", indent);
    }
    if (functions.is_noexcept[function_index]) {
        write("\n{}  :noexcept true", indent);
    }
    if (functions.is_static[function_index]) {
        write("\n{}  :static true", indent);
    }
    if (functions.is_constexpr[function_index]) {
        write("\n{}  :constexpr true", indent);
    }
    if (functions.is_nodiscard[function_index]) {
        write("\n{}  :nodiscard true", indent);
    }
    if (functions.is_inline[function_index]) {
        write("\n{}  :inline true", indent);
    }
    if (functions.definition_in_source[function_index]) {
        write("\n{}  :definition-in-source true", indent);
    }
    if (functions.template_parameters[function_index].has_value()) {
        write("\n{}  :template-parameters {}",
              indent,
              quote(text(*functions.template_parameters[function_index])));
    }
    if (functions.requires_clause[function_index].has_value()) {
        write(
            "\n{}  :requires {}", indent, quote(text(*functions.requires_clause[function_index])));
    }
    for (auto const& parameter : items(functions.parameters[function_index])) {
        auto const parameter_index{parameters.index(parameter)};
        write("\n{}  (parameter {} {}",
              indent,
              text(parameters.name[parameter_index]),
              render_type_ref(parameters.type[parameter_index]));
        if (parameters.default_value[parameter_index].has_value()) {
            write(" :default {}", quote(*parameters.default_value[parameter_index]));
        }
        output += ')';
    }
    output += ')';
}
auto CanonicalRenderer::render_semantic_relation(
    lispb::codegen::SemanticRelationId const& relationship) -> std::string {
    auto const& semantic_relations{store_.semantic_relations};
    auto const relationship_index{semantic_relations.index(relationship)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(relation {} {}",
          semantic_relation_kind_name(semantic_relations.kind[relationship_index]),
          render_type_ref(semantic_relations.target[relationship_index]));
    if (semantic_relations.unit[relationship_index].has_value()) {
        write(" :unit {}",
              semantic_relation_unit_name(*semantic_relations.unit[relationship_index]));
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_packed_segment(codegen::PackedSegmentId const segment)
    -> std::string {
    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    [&]() {
        auto const value{segment};
        switch (value.kind()) {
            case lispb::codegen::PackedSegmentKind::field: {
                auto const& fields{store_.packed_fields};
                auto const field_index{fields.index(value)};

                write("(field {} {} :bits ",
                      text(fields.name[field_index]),
                      render_type_ref(fields.type[field_index]));
                if (fields.bits[field_index].has_value()) {
                    write("{}", *fields.bits[field_index]);
                } else {
                    output += "auto";
                }
                if (fields.kind[field_index] != codegen::PackedFieldKind::unsigned_integer) {
                    write(" :kind {}", packed_field_kind_name(fields.kind[field_index]));
                }
                if (fields.range_helper[field_index]) {
                    output += " :range-helper true";
                }
                if (fields.default_value[field_index].has_value()) {
                    write(" :default {}",
                          format_packed_integer(*fields.default_value[field_index]));
                }
                if (fields.minimum_value[field_index].has_value()) {
                    write(" :minimum {}",
                          format_packed_integer(*fields.minimum_value[field_index]));
                }
                if (fields.maximum_value[field_index].has_value()) {
                    write(" :maximum {}",
                          format_packed_integer(*fields.maximum_value[field_index]));
                }
                for (auto const code : items(fields.named_codes[field_index])) {
                    write("\n      {}", render_named_code(code));
                }
                if (fields.relationship[field_index].has_value()) {
                    write("\n      {}",
                          render_semantic_relation(*fields.relationship[field_index]));
                }
                break;
            }
            case lispb::codegen::PackedSegmentKind::reserved: {
                auto const& reserved_bits{store_.packed_reserved_bits};
                auto const reserved_index{reserved_bits.index(value)};

                write("(reserved {} :bits {}",
                      text(reserved_bits.name[reserved_index]),
                      reserved_bits.bits[reserved_index]);
                break;
            }
            default:
                std::unreachable();
        }
        output += ')';
    }();
    return output;
}
auto CanonicalRenderer::render_packed_value(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& packed_values{store_.packed_values};
    auto const schema_index{packed_values.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(packed-value {}\n    :storage {}",
          text(packed_values.name[schema_index]),
          render_type_ref(packed_values.storage_type[schema_index]));
    if (packed_values.byte_order[schema_index].has_value()) {
        write("\n    :byte-order {}",
              packed_byte_order_name(*packed_values.byte_order[schema_index]));
    }
    if (packed_values.bit_order[schema_index].has_value()) {
        write("\n    :bit-order {}", packed_bit_order_name(*packed_values.bit_order[schema_index]));
    }
    if (packed_values.invalid_value[schema_index].has_value()) {
        write("\n    :invalid-value {}", *packed_values.invalid_value[schema_index]);
    }
    if (packed_values.export_specifier[schema_index].value != 0) {
        write("\n    :export-specifier {}", text(packed_values.export_specifier[schema_index]));
    }
    if (packed_values.mutable_value[schema_index]) {
        output += "\n    :mutable true";
    }
    for (auto const& segment : items(packed_values.segments[schema_index])) {
        write("\n    {}", render_packed_segment(segment));
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_named_code(lispb::codegen::PackedNamedCodeId const& code)
    -> std::string {
    auto const& packed_named_codes{store_.packed_named_codes};
    auto const code_index{packed_named_codes.index(code)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(code {} :value {}",
          text(packed_named_codes.name[code_index]),
          format_packed_integer(packed_named_codes.value[code_index]));
    if (packed_named_codes.sentinel[code_index]) {
        output += " :sentinel true";
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_integer_scalar(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& integer_scalars{store_.integer_scalars};
    auto const schema_index{integer_scalars.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(integer-scalar {}\n    :signed {}\n    :minimum {}\n    :maximum {}\n    :bit-width ",
          text(integer_scalars.name[schema_index]),
          (integer_scalars.signedness[schema_index] ? "true" : "false"),
          format_packed_integer(integer_scalars.minimum_value[schema_index]),
          format_packed_integer(integer_scalars.maximum_value[schema_index]));
    if (integer_scalars.bit_width[schema_index].has_value()) {
        write("{}", *integer_scalars.bit_width[schema_index]);
    } else {
        output += "auto";
    }
    if (integer_scalars.cpp_emission[schema_index] != codegen::IntegerScalarCppEmission::none) {
        write("\n    :cpp-emission {}",
              integer_scalar_cpp_emission_name(integer_scalars.cpp_emission[schema_index]));
    }
    if (integer_scalars.cpp_type[schema_index].has_value()) {
        write("\n    :cpp-type {}", render_type_ref(*integer_scalars.cpp_type[schema_index]));
    }
    for (auto const& code : items(integer_scalars.named_codes[schema_index])) {
        write("\n    {}", render_named_code(code));
    }
    if (integer_scalars.relationship[schema_index].has_value()) {
        write("\n    {}", render_semantic_relation(*integer_scalars.relationship[schema_index]));
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_linear_quantized(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& linear_quantizeds{store_.linear_quantizeds};
    auto const schema_index{linear_quantizeds.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write(
        "(linear-quantized {}\n    :source {}\n    :bits {}\n    :reserved-codes {}\n    :clipping "
        "{})",
        text(linear_quantizeds.name[schema_index]),
        render_type_ref(linear_quantizeds.source[schema_index]),
        linear_quantizeds.bit_width[schema_index],
        linear_quantizeds.reserved_codes[schema_index],
        quantization_clipping_name(linear_quantizeds.clipping[schema_index]));
    return output;
}
auto CanonicalRenderer::render_integer_varint(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& integer_varints{store_.integer_varints};
    auto const schema_index{integer_varints.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(integer-varint {}\n    :source {}\n    :encoding {})",
          text(integer_varints.name[schema_index]),
          render_type_ref(integer_varints.source[schema_index]),
          integer_varint_encoding_name(integer_varints.encoding[schema_index]));
    return output;
}
auto CanonicalRenderer::render_fixed_point(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& fixed_points{store_.fixed_points};
    auto const schema_index{fixed_points.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(fixed-point {}\n    :signed {}\n    :total-bits {}\n    :fractional-bits {}\n    "
          ":rounding {}",
          text(fixed_points.name[schema_index]),
          (fixed_points.signedness[schema_index] ? "true" : "false"),
          fixed_points.total_bits[schema_index],
          fixed_points.fractional_bits[schema_index],
          fixed_point_rounding_name(fixed_points.rounding[schema_index]));
    if (fixed_points.minimum_value[schema_index].has_value()) {
        write("\n    :minimum {}", text(*fixed_points.minimum_value[schema_index]));
    }
    if (fixed_points.maximum_value[schema_index].has_value()) {
        write("\n    :maximum {}", text(*fixed_points.maximum_value[schema_index]));
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_mini_float(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& mini_floats{store_.mini_floats};
    auto const schema_index{mini_floats.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(mini-float {}\n    :sign-bits {}\n    :exponent-bits {}\n    "
          ":significand-bits {}\n    :bias {})",
          text(mini_floats.name[schema_index]),
          mini_floats.sign_bits[schema_index],
          mini_floats.exponent_bits[schema_index],
          mini_floats.significand_bits[schema_index],
          mini_floats.exponent_bias[schema_index]);
    return output;
}
auto CanonicalRenderer::render_optional_sentinel(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& optional_sentinels{store_.optional_sentinels};
    auto const schema_index{optional_sentinels.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(optional-sentinel {}\n    :source {}\n    :sentinel {})",
          text(optional_sentinels.name[schema_index]),
          render_type_ref(optional_sentinels.source[schema_index]),
          text(optional_sentinels.sentinel[schema_index]));
    return output;
}
auto
    CanonicalRenderer::render_optional_presence_bit(lispb::codegen::DeclarationHandle const& schema)
        -> std::string {
    auto const& optional_presence_bits{store_.optional_presence_bits};
    auto const schema_index{optional_presence_bits.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(optional-presence-bit {}\n    :source {})",
          text(optional_presence_bits.name[schema_index]),
          render_type_ref(optional_presence_bits.source[schema_index]));
    return output;
}
auto CanonicalRenderer::render_record(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& records{store_.records};
    auto const schema_index{records.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(record {}", text(records.name[schema_index]));
    if (records.export_specifier[schema_index].value != 0) {
        write("\n    :export-specifier {}", text(records.export_specifier[schema_index]));
    }
    if (records.comparison[schema_index] != codegen::RecordComparison::none) {
        write("\n    :comparison {}",
              (records.comparison[schema_index] == codegen::RecordComparison::equality
                   ? "equality"
                   : "three-way"));
    }
    if (records.comparison_noexcept[schema_index]) {
        output += "\n    :comparison-noexcept true";
    }
    for (auto const& member : items(records.members[schema_index])) {
        write("\n    {}", render_aggregate_child(store_.record_members, "member", member));
    }
    for (auto const& function : items(records.functions[schema_index])) {
        output += '\n';
        render_function(output, function, "    ");
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_union(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& unions{store_.unions};
    auto const schema_index{unions.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(union {}", text(unions.name[schema_index]));
    if (unions.export_specifier[schema_index].value != 0) {
        write("\n    :export-specifier {}", text(unions.export_specifier[schema_index]));
    }
    for (auto const& alternative : items(unions.alternatives[schema_index])) {
        write("\n    {}",
              render_aggregate_child(store_.union_alternatives, "alternative", alternative));
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_tagged_union_alternative(
    lispb::codegen::TaggedUnionAlternativeId const& alternative) -> std::string {
    auto const& tagged_union_alternatives{store_.tagged_union_alternatives};
    auto const alternative_index{tagged_union_alternatives.index(alternative)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(alternative {} {} :tag {}",
          text(tagged_union_alternatives.name[alternative_index]),
          render_type_ref(tagged_union_alternatives.type[alternative_index]),
          text(tagged_union_alternatives.tag[alternative_index]));
    if (tagged_union_alternatives.count[alternative_index].has_value()) {
        write(" :count {}", *tagged_union_alternatives.count[alternative_index]);
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_tagged_union(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& tagged_unions{store_.tagged_unions};
    auto const schema_index{tagged_unions.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(tagged-union {}\n    :discriminant {}",
          text(tagged_unions.name[schema_index]),
          render_type_ref(tagged_unions.discriminant[schema_index]));
    if (tagged_unions.export_specifier[schema_index].value != 0) {
        write("\n    :export-specifier {}", text(tagged_unions.export_specifier[schema_index]));
    }
    for (auto const& alternative : items(tagged_unions.alternatives[schema_index])) {
        write("\n    {}", render_tagged_union_alternative(alternative));
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_soa_member(lispb::codegen::SoaMemberId const& member)
    -> std::string {
    auto const& dimensions{store_.soa_mask_dimensions};
    auto const& soa_members{store_.soa_members};
    auto const member_index{soa_members.index(member)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(member {} {} {}",
          text(soa_members.name[member_index]),
          soa_member_kind_name(soa_members.kind[member_index]),
          render_type_ref(soa_members.type[member_index]));
    if (soa_members.fixed_schema[member_index].has_value()) {
        write("\n      :fixed-schema {}", text(*soa_members.fixed_schema[member_index]));
    }
    if (soa_members.nested_schema[member_index].has_value()) {
        write("\n      :nested-schema {}", text(*soa_members.nested_schema[member_index]));
    }
    if (soa_members.mask_field[member_index]) {
        output += "\n      :mask-field true";
    }
    auto const mask_dimensions{items(soa_members.mask_dimensions[member_index])};
    auto const dimension_count{mask_dimensions.size()};
    if (!mask_dimensions.empty()) {
        output += "\n      :mask-dimensions (";
        for (std::size_t index{}; index < dimension_count; ++index) {
            auto const dimension{mask_dimensions[index]};
            auto const dimension_index{dimensions.index(dimension)};
            write("{}({} {})",
                  (index == 0 ? "" : " "),
                  text(dimensions.index_name[dimension_index]),
                  quote(dimensions.extent[dimension_index]));
        }
        output += ')';
    }
    if (soa_members.relationship[member_index].has_value()) {
        write("\n      {}", render_semantic_relation(*soa_members.relationship[member_index]));
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_soa(lispb::codegen::DeclarationHandle const& schema) -> std::string {
    auto const& soas{store_.soas};
    auto const schema_index{soas.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(struct {}", text(soas.name[schema_index]));
    if (soas.view_name[schema_index].has_value()) {
        write("\n    :view-name {}", text(*soas.view_name[schema_index]));
    }
    if (soas.const_view_name[schema_index].has_value()) {
        write("\n    :const-view-name {}", text(*soas.const_view_name[schema_index]));
    }
    auto const operations{items(soas.operations[schema_index])};
    if (!operations.empty()) {
        auto const operations_count{operations.size()};
        output += "\n    :operations (";
        for (std::size_t index{}; index < operations_count; ++index) {
            write("{}{}", (index == 0 ? "" : " "), storage_operation_name(operations[index]));
        }
        output += ')';
    }
    if (soas.export_specifier[schema_index].value != 0) {
        write("\n    :export-specifier {}", text(soas.export_specifier[schema_index]));
    }
    if (!texts(soas.using_declarations[schema_index]).empty()) {
        output += "\n    :using-declarations ";
        render_quoted_list(output, texts(soas.using_declarations[schema_index]));
    }
    if (soas.equivalent_type[schema_index].has_value()) {
        write("\n    :equivalent-type {}", render_type_ref(*soas.equivalent_type[schema_index]));
    }
    auto const components{texts(soas.vector_components[schema_index])};
    if (!components.empty()) {
        auto const components_count{components.size()};
        output += "\n    :vector-components (";
        for (std::size_t index{}; index < components_count; ++index) {
            if (index != 0) {
                output += ' ';
            }
            write("{}", components[index]);
        }
        output += ')';
    }
    if (soas.copy_element_memberwise[schema_index]) {
        output += "\n    :copy-element-memberwise true";
    }
    if (soas.layout_only[schema_index]) {
        output += "\n    :layout-only true";
    }
    if (soas.storage[schema_index]) {
        write("\n    :storage {}", codegen::soa_storage_name(*soas.storage[schema_index]));
    }
    if (soas.field_mask_name[schema_index].has_value()) {
        write("\n    :field-mask-name {}", text(*soas.field_mask_name[schema_index]));
    }
    if (soas.field_enum_name[schema_index].has_value()) {
        write("\n    :field-enum-name {}", text(*soas.field_enum_name[schema_index]));
    }
    for (auto const& member : items(soas.members[schema_index])) {
        write("\n    {}", render_soa_member(member));
    }
    for (auto const& function : items(soas.functions[schema_index])) {
        output += '\n';
        render_function(output, function, "    ");
    }
    for (auto const& function : items(soas.const_view_functions[schema_index])) {
        output += '\n';
        render_function(output, function, "    ", "view-function");
    }
    for (auto const& function : items(soas.mutable_view_functions[schema_index])) {
        output += '\n';
        render_function(output, function, "    ", "mutable-view-function");
    }
    if (soas.array_allocator[schema_index]) {
        write("\n    :array-allocator {}", render_type_ref(*soas.array_allocator[schema_index]));
    }
    if (soas.fixed[schema_index].has_value()) {
        auto const& fixed_soas{store_.fixed_soas};
        auto const fixed_index{fixed_soas.index(*soas.fixed[schema_index])};

        write("\n    (fixed {}", text(fixed_soas.storage_name[fixed_index]));
        auto const containers{texts(fixed_soas.containers[fixed_index])};
        if (!containers.empty()) {
            auto const container_count{containers.size()};
            output += " :containers (";
            for (std::size_t index{}; index < container_count; ++index) {
                write("{}{}", (index == 0 ? "" : " "), containers[index]);
            }
            output += ')';
        }
        output += ')';
    }
    if (soas.single_allocation[schema_index].has_value()) {
        write("\n    (single-allocation {}", text(*soas.single_allocation[schema_index]));
        output += ')';
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_vector_soa(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& vector_soas{store_.vector_soas};
    auto const schema_index{vector_soas.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(vector-soa {}\n    :value-type {}\n    :components ",
          text(vector_soas.name[schema_index]),
          render_type_ref(vector_soas.value_type[schema_index]));
    render_quoted_list(output, texts(vector_soas.components[schema_index]));
    if (!texts(vector_soas.equivalent_members[schema_index]).empty()) {
        output += "\n    :equivalent-members ";
        render_quoted_list(output, texts(vector_soas.equivalent_members[schema_index]));
    }
    if (vector_soas.equivalent_accessors[schema_index]) {
        output += "\n    :equivalent-accessors true";
    }
    if (vector_soas.equivalent_constructor[schema_index].has_value()) {
        write("\n    :equivalent-constructor {}",
              text(*vector_soas.equivalent_constructor[schema_index]));
    }
    write("\n    :equivalent-type {}", render_type_ref(vector_soas.equivalent_type[schema_index]));
    if (vector_soas.export_specifier[schema_index].value != 0) {
        write("\n    :export-specifier {}", text(vector_soas.export_specifier[schema_index]));
    }
    if (vector_soas.fixed[schema_index].has_value()) {
        auto const& fixed_soas{store_.fixed_soas};
        auto const fixed_index{fixed_soas.index(*vector_soas.fixed[schema_index])};

        write("\n    (fixed {}", text(fixed_soas.storage_name[fixed_index]));
        auto const containers{texts(fixed_soas.containers[fixed_index])};
        if (!containers.empty()) {
            output += " :containers ";
            render_quoted_list(output, containers);
        }
        output += ')';
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_homogeneous_layout(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& homogeneous_values{store_.homogeneous_values};
    auto const& homogeneous_layouts{store_.homogeneous_layouts};
    auto const schema_index{homogeneous_layouts.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(layout {}\n    :components ", text(homogeneous_layouts.name[schema_index]));
    render_quoted_list(output, texts(homogeneous_layouts.components[schema_index]));
    if (!texts(homogeneous_layouts.input_members[schema_index]).empty()) {
        output += "\n    :input-members ";
        render_quoted_list(output, texts(homogeneous_layouts.input_members[schema_index]));
    }
    if (homogeneous_layouts.export_specifier[schema_index].value != 0) {
        write("\n    :export-specifier {}",
              text(homogeneous_layouts.export_specifier[schema_index]));
    }
    for (auto const& value : items(homogeneous_layouts.value_types[schema_index])) {
        auto const value_index{homogeneous_values.index(value)};
        write("\n    (value-type {} {}",
              render_type_ref(homogeneous_values.type[value_index]),
              text(homogeneous_values.suffix[value_index]));
        if (homogeneous_values.equivalent_type[value_index].has_value()) {
            write("\n      :equivalent-type {}",
                  render_type_ref(*homogeneous_values.equivalent_type[value_index]));
        }
        auto const input_types{items(homogeneous_values.input_types[value_index])};
        if (!input_types.empty()) {
            auto const input_count{input_types.size()};
            output += "\n      :input-types (";
            for (std::size_t index{}; index < input_count; ++index) {
                write("{}{}", (index == 0 ? "" : " "), render_type_ref(input_types[index]));
            }
            output += ')';
        }
        output += ')';
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_static_table(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& static_table_groups{store_.static_table_groups};
    auto const& static_table_columns{store_.static_table_columns};
    auto const& static_tables{store_.static_tables};
    auto const schema_index{static_tables.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(table {}", text(static_tables.name[schema_index]));
    if (static_tables.export_specifier[schema_index].value != 0) {
        write("\n    :export-specifier {}", text(static_tables.export_specifier[schema_index]));
    }
    for (auto const& row : items(static_tables.rows[schema_index])) {
        write("\n    (row {})",
              text(store_.static_table_rows.name[store_.static_table_rows.index(row)]));
    }
    for (auto const& column : items(static_tables.columns[schema_index])) {
        auto const column_index{static_table_columns.index(column)};
        write("\n    (column {} {})",
              text(static_table_columns.name[column_index]),
              render_type_ref(static_table_columns.type[column_index]));
    }
    for (auto const& group : items(static_tables.groups[schema_index])) {
        auto const group_index{static_table_groups.index(group)};
        write("\n    (group {} {} :columns ",
              text(static_table_groups.name[group_index]),
              render_type_ref(static_table_groups.type[group_index]));
        render_quoted_list(output, texts(static_table_groups.columns[group_index]));
        output += ')';
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_facade(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& parameters{store_.parameters};
    auto const& facade_methods{store_.facade_methods};
    auto const& facades{store_.facades};
    auto const schema_index{facades.index(schema)};

    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(facade {} {} {}",
          text(facades.name[schema_index]),
          render_type_ref(facades.target_type[schema_index]),
          text(facades.target_member_name[schema_index]));
    if (!texts(facades.validation_lines[schema_index]).empty()) {
        output += "\n    :validation ";
        render_quoted_list(output, texts(facades.validation_lines[schema_index]));
    }
    if (!texts(facades.validation_dependencies[schema_index]).empty()) {
        output += "\n    :validation-dependencies ";
        render_quoted_list(output, texts(facades.validation_dependencies[schema_index]));
    }
    if (facades.export_specifier[schema_index].value != 0) {
        write("\n    :export-specifier {}", text(facades.export_specifier[schema_index]));
    }
    if (access_keyword(facades.bind_access[schema_index]) != "public") {
        write("\n    :bind-access {}", access_keyword(facades.bind_access[schema_index]));
    }
    if (access_keyword(facades.method_access[schema_index]) != "public") {
        write("\n    :method-access {}", access_keyword(facades.method_access[schema_index]));
    }
    if (!texts(facades.friends[schema_index]).empty()) {
        output += "\n    :friends ";
        render_quoted_list(output, texts(facades.friends[schema_index]));
    }
    if (codegen::record_keyword(facades.friend_kind[schema_index]) != "class") {
        write("\n    :friend-kind {}", codegen::record_keyword(facades.friend_kind[schema_index]));
    }
    if (facades.definitions_in_source[schema_index]) {
        output += "\n    :definitions-in-source true";
    }
    if (facades.reference_target[schema_index]) {
        output += "\n    :target-storage reference";
    }
    for (auto const& method : items(facades.methods[schema_index])) {
        auto const method_index{facade_methods.index(method)};
        write("\n    (method {} {}",
              text(facade_methods.name[method_index]),
              render_type_ref(facade_methods.return_type[method_index]));
        if (facade_methods.is_const[method_index]) {
            output += "\n      :const true";
        }
        if (facade_methods.is_noexcept[method_index]) {
            output += "\n      :noexcept true";
        }
        if (facade_methods.target_name[method_index].has_value()) {
            write("\n      :target-name {}", text(*facade_methods.target_name[method_index]));
        }
        for (auto const& parameter : items(facade_methods.parameters[method_index])) {
            auto const parameter_index{parameters.index(parameter)};
            write("\n      (parameter {} {}",
                  text(parameters.name[parameter_index]),
                  render_type_ref(parameters.type[parameter_index]));
            if (parameters.default_value[parameter_index].has_value()) {
                write(" :default {}", quote(*parameters.default_value[parameter_index]));
            }
            output += ')';
        }
        output += ')';
    }
    output += ')';
    return output;
}
auto CanonicalRenderer::render_settings_schema(lispb::codegen::DeclarationHandle const& schema)
    -> std::string {
    auto const& schemas{store_.settings_schemas};
    auto const row{schemas.index(schema)};
    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("(settings-schema {}\n    :target {}\n    :target-header {}",
          text(schemas.name[row]),
          text(schemas.target[row]),
          quote(text(schemas.target_header[row])));
    auto const paths{items(schemas.paths[row])};
    auto const members{items(schemas.members[row])};
    auto const count{paths.size()};
    for (std::size_t index{}; index < count; ++index) {
        write("\n    (field {} {})", quote(text(paths[index])), quote(text(members[index])));
    }
    output += ')';
    return output;
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
    auto const& soa_allocator_variants{store_.soa_allocator_variants};
    auto const settings{store_.module_settings(module)};
    auto const& settings_store{store_.module_settings_store};
    auto const settings_index{settings_store.index(settings)};
    auto const head{module.kind() == codegen::ModuleKind::normal     ? "module"
                    : module.kind() == codegen::ModuleKind::settings ? "settings-module"
                                                                     : "umbrella-module"};
    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("({} {}\n  :header {}",
          head,
          text(settings_store.name[settings_index]),
          quote(text(settings_store.header[settings_index])));
    if (settings_store.source[settings_index].has_value()) {
        write("\n  :source {}", quote(text(*settings_store.source[settings_index])));
    }
    if (settings_store.header_include[settings_index].has_value()) {
        write("\n  :header-include {}", quote(*settings_store.header_include[settings_index]));
    }
    if (settings_store.namespace_name[settings_index].has_value()) {
        write("\n  :namespace {}", text(*settings_store.namespace_name[settings_index]));
    }
    if (!settings_store.include_order[settings_index].empty()) {
        output += "\n  :include-order ";
        render_quoted_list(output, texts(settings_store.include_order[settings_index]));
    }
    if (!settings_store.prelude_lines[settings_index].empty()) {
        output += "\n  :prelude ";
        render_quoted_list(output, texts(settings_store.prelude_lines[settings_index]));
    }

    if (module.kind() == codegen::ModuleKind::umbrella) {
        auto const& umbrella_modules{store_.umbrella_modules};
        auto const umbrella_index{umbrella_modules.index(module)};

        output += "\n  :headers ";
        render_quoted_list(output, texts(umbrella_modules.headers[umbrella_index]));
        output += ')';
        return output;
    }
    if (module.kind() == codegen::ModuleKind::settings) {
        render_settings_module(output, module);
        output += ')';
        return output;
    }
    auto const& modules{store_.normal_modules};
    auto const module_index{modules.index(module)};

    if (modules.enum_helper_namespace[module_index].has_value()) {
        write("\n  :helper-namespace {}", text(*modules.enum_helper_namespace[module_index]));
    }
    if (modules.soa_backend[module_index] == codegen::SoaBackend::standard_library) {
        output += "\n  :backend standard-library";
    }
    for (auto const& allocator : items(modules.soa_array_allocators[module_index])) {
        auto const allocator_index{soa_allocator_variants.index(allocator)};
        write("\n  (array-allocator {} {})",
              text(soa_allocator_variants.prefix[allocator_index]),
              render_type_ref(soa_allocator_variants.allocator[allocator_index]));
    }
    for (auto const& declaration : items(modules.declarations[module_index])) {
        write("\n  {}", render_declaration_schema(declaration));
    }
    output += ')';
    return output;
}
void CanonicalRenderer::render_settings_module(std::string& output,
                                               codegen::ModuleHandle const module) {
    auto const& settings{store_.settings};
    auto const& controls{store_.setting_controls};
    auto const& categories{store_.setting_categories};
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    auto const& modules{store_.settings_modules};
    auto const index{modules.index(module)};
    write("\n  :api-name {}\n  :state-name {}",
          text(modules.api_name[index]),
          text(modules.state_name[index]));
    if (modules.export_specifier[index].value != 0) {
        write("\n  :export-specifier {}", text(modules.export_specifier[index]));
    }
    for (auto const category : items(modules.categories[index])) {
        auto const category_index{categories.index(category)};
        write("\n  (category {} {})",
              text(categories.name[category_index]),
              quote(categories.label[category_index]));
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
        write("\n  (setting {} {}\n    :category {}\n    :value-type {}\n    :backend "
              "{}\n    :apply {}\n    :device {}",
              text(settings.name[row]),
              quote(settings.label[row]),
              text(settings.category[row]),
              render_type_ref(settings.value_type[row]),
              text(settings.backend[row]),
              apply,
              device);
        if (settings.tooltip[row]) {
            write("\n    :tooltip {}", quote(*settings.tooltip[row]));
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
        write("\n    (control {}", kind);
        if (controls.options_provider[control]) {
            write(" :options-provider {}", quote(*controls.options_provider[control]));
        }
        if (controls.availability_provider[control]) {
            write(" :availability-provider {}", quote(*controls.availability_provider[control]));
        }
        if (controls.minimum[control]) {
            write(" :min {:.17g}", *controls.minimum[control]);
        }
        if (controls.maximum[control]) {
            write(" :max {:.17g}", *controls.maximum[control]);
        }
        if (controls.step[control]) {
            write(" :step {:.17g}", *controls.step[control]);
        }
        if (controls.custom_row[control]) {
            write(" :custom-row {}", quote(*controls.custom_row[control]));
        }
        output += "))";
    }
}

} // namespace lispb::schema::detail
