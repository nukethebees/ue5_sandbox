#include "schema_parser.h"

#include <codegen/ast/access.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/parameter_passing.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/type_operation.h>
#include <codegen/schema.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/external_scalar_id.h>
#include <codegen/schema/external_scalar_kind.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/floating_point_format.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/native_enum_helper.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_byte_order.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_segment_id.h>
#include <codegen/schema/quantization_clipping.h>
#include <codegen/schema/record_comparison.h>
#include <codegen/schema/registered_types.h>
#include <codegen/schema/schema_property.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <codegen/schema/setting_apply_mode.h>
#include <codegen/schema/setting_control_kind.h>
#include <codegen/schema/setting_device.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/type_registry.h>
#include <codegen/sexpr/fields.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {
using sexpr::Fields;
using sexpr::FormId;
using P = SchemaProperty;

namespace {
template <typename Enum>
auto parse_keyword(sexpr::Parser const& parser,
                   FormId const form,
                   std::string_view const purpose,
                   std::optional<Enum> (*lookup)(std::string_view) noexcept) -> Enum {
    auto const name{parser.text(form, purpose)};
    if (auto const value{lookup(name)}) {
        return *value;
    }
    parser.fail(form, std::format("unknown {} '{}'", purpose, name));
}

auto bounded_integer(sexpr::Parser const& parser,
                     FormId const form,
                     std::string_view const purpose,
                     int const minimum,
                     int const maximum) -> int {
    auto const value{parser.integer(form, purpose)};
    if (value < minimum || value > maximum) {
        parser.fail(form, std::format("{} must be in the range {}..{}", purpose, minimum, maximum));
    }
    return value;
}
}

SchemaParser::SchemaParser(sexpr::Syntax& syntax)
    : syntax_{syntax}
    , parser_{syntax} {}

auto SchemaParser::optional_text_id(sexpr::Fields const& fields, SchemaProperty const key)
    -> std::optional<lispb::StringId> {
    auto const name{to_serialized_string(key)};
    auto const value{fields.optional(name)};
    return value ? std::optional{parser_.text_id(*value, name)} : std::nullopt;
}

auto SchemaParser::parse_export_specifier(sexpr::Fields const& fields) -> lispb::StringId {
    auto const value{fields.optional(P::export_specifier)};
    if (!value) {
        return {};
    }
    auto const result{parser_.identifier(*value, "export specifier")};
    if (result.value == 0) {
        parser_.fail(*value, "export specifier cannot be empty");
    }
    return result;
}

auto SchemaParser::optional_identifier(sexpr::Fields const& fields, SchemaProperty const key)
    -> std::optional<lispb::StringId> {
    auto const name{to_serialized_string(key)};
    auto const value{fields.optional(name)};
    return value ? std::optional{parser_.identifier(*value, name)} : std::nullopt;
}

auto SchemaParser::text_id_list(sexpr::FormId const form, std::string_view const purpose)
    -> std::vector<lispb::StringId> {
    if (!syntax_.is_list(form)) {
        parser_.fail(form, std::format("{} must be a list", purpose));
    }
    std::vector<lispb::StringId> result;
    result.reserve(syntax_.children(form).size());
    for (auto const child : syntax_.children(form)) {
        result.push_back(parser_.text_id(child, purpose));
    }
    return result;
}

auto SchemaParser::identifier_list(sexpr::FormId const form, std::string_view const purpose)
    -> std::vector<lispb::StringId> {
    if (!syntax_.is_list(form)) {
        parser_.fail(form, std::format("{} must be a list", purpose));
    }
    std::vector<lispb::StringId> result;
    result.reserve(syntax_.children(form).size());
    for (auto const child : syntax_.children(form)) {
        result.push_back(parser_.identifier(child, purpose));
    }
    return result;
}

auto SchemaParser::text_id_list_or(sexpr::Fields const& fields, SchemaProperty const key)
    -> std::vector<lispb::StringId> {
    auto const name{to_serialized_string(key)};
    auto const value{fields.optional(name)};
    return value ? text_id_list(*value, name) : std::vector<lispb::StringId>{};
}

auto SchemaParser::identifier_list_or(sexpr::Fields const& fields, SchemaProperty const key)
    -> std::vector<lispb::StringId> {
    auto const name{to_serialized_string(key)};
    auto const value{fields.optional(name)};
    return value ? identifier_list(*value, name) : std::vector<lispb::StringId>{};
}

auto SchemaParser::unsigned_integer(sexpr::FormId const form, std::string_view const purpose)
    -> std::uint64_t {
    auto const value{std::string{parser_.text(form, purpose)}};
    if (value.empty() || value.front() == '-') {
        parser_.fail(form, std::format("{} must be an unsigned integer", purpose));
    }
    try {
        std::size_t parsed{};
        auto const result{std::stoull(value, &parsed, 0)};
        if (parsed != value.size()) {
            parser_.fail(form, std::format("{} must be an unsigned integer", purpose));
        }
        return result;
    } catch (std::invalid_argument const&) {
        parser_.fail(form, std::format("{} must be an unsigned integer", purpose));
    } catch (std::out_of_range const&) {
        parser_.fail(form, std::format("{} is out of range", purpose));
    }
}

auto SchemaParser::packed_integer(sexpr::FormId const form, std::string_view const purpose)
    -> PackedIntegerValue {
    auto value{std::string{parser_.text(form, purpose)}};
    auto negative{false};
    if (!value.empty() && (value.front() == '-' || value.front() == '+')) {
        negative = value.front() == '-';
        value.erase(value.begin());
    }
    if (value.empty()) {
        parser_.fail(form, std::format("{} must be an integer", purpose));
    }
    try {
        std::size_t parsed{};
        auto const magnitude{std::stoull(value, &parsed, 0)};
        if (parsed != value.size()) {
            parser_.fail(form, std::format("{} must be an integer", purpose));
        }
        return PackedIntegerValue::from_parts(negative, magnitude);
    } catch (std::invalid_argument const&) {
        parser_.fail(form, std::format("{} must be an integer", purpose));
    } catch (std::out_of_range const&) {
        parser_.fail(form, std::format("{} is out of range", purpose));
    }
}

auto SchemaParser::optional_text(sexpr::Fields const& fields, SchemaProperty const key)
    -> std::optional<std::string> {
    auto const name{to_serialized_string(key)};
    auto const value{fields.optional(name)};
    return !value.has_value() ? std::nullopt
                              : std::optional<std::string>{std::string{parser_.text(*value, name)}};
}

auto SchemaParser::cpp_lines_or(sexpr::Fields const& fields, SchemaProperty const key)
    -> std::vector<lispb::StringId> {
    auto const name{to_serialized_string(key)};
    auto const value{fields.optional(name)};
    if (!value.has_value()) {
        return {};
    }
    if (syntax_.kind(*value) != sexpr::TokenKind::raw_literal) {
        return text_id_list(*value, name);
    }

    if (syntax_.tag(*value) != "cpp") {
        parser_.fail(
            *value,
            std::format("{} requires a #cpp raw literal; got #{}", name, syntax_.tag(*value)));
    }
    if (syntax_.text(*value).empty()) {
        return {};
    }
    return {syntax_.text_id(*value)};
}

auto SchemaParser::boolean_or(sexpr::Fields const& fields,
                              SchemaProperty const key,
                              bool const fallback) -> bool {
    auto const name{to_serialized_string(key)};
    auto const value{fields.optional(name)};
    return !value.has_value() ? fallback : parser_.boolean(*value, name);
}

auto SchemaParser::parse_type_ref(Declarations& declarations, sexpr::FormId const form)
    -> TypeRefId {
    if (!syntax_.is_list(form)) {
        auto& records{declarations.type_refs};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = parser_.identifier(form, "type reference");
        records.suffix[record_index] = {};
        records.nested[record_index] = std::nullopt;
        return record_id;
    }
    Fields const fields{parser_, form, "type-ref", 1};
    fields.validate({P::suffix, P::nested});
    auto& records{declarations.type_refs};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "type reference name");
    records.suffix[record_index] = optional_text_id(fields, P::suffix).value_or(lispb::StringId{});
    records.nested[record_index] = optional_text_id(fields, P::nested);
    return record_id;
}

auto SchemaParser::type_ref_list(Declarations& declarations,
                                 sexpr::FormId const form,
                                 std::string_view const purpose) -> std::vector<TypeRefId> {
    if (!syntax_.is_list(form)) {
        parser_.fail(form, std::format("{} must be a list", purpose));
    }
    std::vector<TypeRefId> result;
    result.reserve(syntax_.children(form).size());
    for (auto const& child : syntax_.children(form)) {
        result.push_back(parse_type_ref(declarations, child));
    }
    return result;
}

auto SchemaParser::parse_parameter(Declarations& declarations, sexpr::FormId const form)
    -> ParameterId {
    Fields const fields{parser_, form, "parameter", 2};
    fields.validate({P::default_});
    auto& records{declarations.parameters};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.type[record_index] = parse_type_ref(declarations, fields.positional(1));
    records.name[record_index] = parser_.identifier(fields.positional(0), "parameter name");
    records.default_value[record_index] = optional_text_id(fields, P::default_);
    return record_id;
}

auto SchemaParser::parse_function(Declarations& declarations,
                                  sexpr::FormId const form,
                                  bool soa_owner) -> FunctionSchemaId {
    Fields const fields{parser_, form, syntax_.head(form), 2};
    fields.validate({P::body,
                     P::append_columns,
                     P::dependencies,
                     P::trailing_return_type,
                     P::const_,
                     P::noexcept_,
                     P::static_,
                     P::inline_,
                     P::constexpr_,
                     P::nodiscard,
                     P::definition_in_source,
                     P::template_parameters,
                     P::requires_},
                    {"parameter"});
    std::vector<ParameterId> parameters;
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        parameters.push_back(parse_parameter(declarations, declaration));
    }
    std::optional<TypeRefId> trailing_return_type;
    if (auto const value{fields.optional(P::trailing_return_type)}) {
        trailing_return_type = parse_type_ref(declarations, *value);
    }
    auto const is_static{boolean_or(fields, P::static_)};
    auto const is_const{
        boolean_or(fields, P::const_, syntax_.head(form) == "view-function" && !is_static)};
    if (syntax_.head(form) == "view-function" && !is_static && !is_const) {
        parser_.fail(
            form, "view-function must be const; use mutable-view-function for a mutable receiver");
    }
    auto body_lines{cpp_lines_or(fields, P::body)};
    if (fields.optional(P::body).has_value() && body_lines.empty()) {
        body_lines.emplace_back();
    }
    if (fields.optional(P::append_columns) && !soa_owner) {
        parser_.fail(form, "append-columns requires an owning SOA function");
    }
    auto& records{declarations.functions};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "function name");
    records.return_type[record_index] = parse_type_ref(declarations, fields.positional(1));
    records.parameters[record_index] = declarations.range(parameters);
    records.body_lines[record_index] = declarations.range(body_lines);
    if (fields.optional(P::append_columns)) {
        records.append_columns[record_index] =
            declarations.range(identifier_list_or(fields, P::append_columns));
    }
    records.dependencies[record_index] =
        declarations.range(identifier_list_or(fields, P::dependencies));
    records.trailing_return_type[record_index] = trailing_return_type;
    records.is_const[record_index] = is_const;
    records.is_noexcept[record_index] = boolean_or(fields, P::noexcept_);
    records.is_static[record_index] = is_static;
    records.is_inline[record_index] = boolean_or(fields, P::inline_);
    records.definition_in_source[record_index] = boolean_or(fields, P::definition_in_source);
    records.template_parameters[record_index] = optional_text_id(fields, P::template_parameters);
    records.requires_clause[record_index] = optional_text_id(fields, P::requires_);
    records.is_constexpr[record_index] = boolean_or(fields, P::constexpr_);
    records.is_nodiscard[record_index] = boolean_or(fields, P::nodiscard);
    return record_id;
}

auto SchemaParser::parse_member(Declarations& declarations, sexpr::FormId const form)
    -> SoaMemberId {
    Fields const fields{parser_, form, "member", 3};
    fields.validate({P::fixed_schema,
                     P::mask_field,
                     P::mask_dimensions,
                     P::equivalent_member,
                     P::equivalent_accessor},
                    {"relation"});
    auto const kind_name{parser_.text(fields.positional(1), "member kind")};
    auto const parsed_kind{try_parse_serialized_soa_member_kind(kind_name)};
    if (!parsed_kind) {
        parser_.fail(fields.positional(1), std::format("unknown SOA member kind '{}'", kind_name));
    }
    auto const kind{*parsed_kind};
    std::vector<SoaMaskDimensionId> mask_dimensions;
    if (auto const dimensions{fields.optional(P::mask_dimensions)}) {
        if (!syntax_.is_list(*dimensions)) {
            parser_.fail(*dimensions, "mask-dimensions must be a list");
        }
        for (auto const& dimension : syntax_.children(*dimensions)) {
            if (!syntax_.is_list(dimension) || syntax_.children(dimension).size() != 2) {
                parser_.fail(dimension,
                             "mask dimension must contain an index name and extent expression");
            }
            {
                auto& records{declarations.soa_mask_dimensions};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.index_name[record_index] =
                    parser_.identifier(syntax_.children(dimension)[0], "mask dimension index name");
                records.extent[record_index] =
                    parser_.text_id(syntax_.children(dimension)[1], "mask dimension extent");
                mask_dimensions.push_back(record_id);
            }
        }
    }
    std::optional<SemanticRelationId> relationship;
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        if (relationship.has_value()) {
            parser_.fail(declaration, "SOA member may have only one relationship");
        }
        Fields const relation{parser_, declaration, "relation", 2};
        relation.validate({P::unit});
        auto const unit{relation.optional(P::unit)};
        {
            auto& records{declarations.semantic_relations};
            auto const record_id{records.append()};
            auto const record_index{records.index(record_id)};
            records.kind[record_index] = parse_keyword(parser_,
                                                       relation.positional(0),
                                                       "semantic relationship kind",
                                                       try_parse_serialized_semantic_relation_kind);
            records.target[record_index] = parse_type_ref(declarations, relation.positional(1));
            records.unit[record_index] = !unit.has_value()
                                           ? std::nullopt
                                           : std::optional{parse_semantic_relation_unit(*unit)};
            relationship = record_id;
        }
    }
    auto& records{declarations.soa_members};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "member name");
    records.kind[record_index] = kind;
    records.type[record_index] = parse_type_ref(declarations, fields.positional(2));
    records.equivalent_member[record_index] = optional_identifier(fields, P::equivalent_member);
    records.equivalent_accessor[record_index] = boolean_or(fields, P::equivalent_accessor);
    records.mask_field[record_index] = boolean_or(fields, P::mask_field);
    records.mask_dimensions[record_index] = declarations.range(mask_dimensions);
    records.relationship[record_index] = relationship;
    return record_id;
}

auto SchemaParser::parse_soa(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "struct", 1};
    fields.validate({P::export_specifier,
                     P::using_declarations,
                     P::equivalent_type,
                     P::equivalent_constructor,
                     P::layout_only,
                     P::field_mask_name,
                     P::field_enum_name},
                    {"member", "function", "view-function", "mutable-view-function"});

    std::vector<SoaMemberId> members;
    std::vector<FunctionSchemaId> functions;
    std::vector<FunctionSchemaId> const_view_functions;
    std::vector<FunctionSchemaId> mutable_view_functions;
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        auto const head{syntax_.head(declaration)};
        if (head == "member") {
            members.push_back(parse_member(declarations, declaration));
        } else if (head == "function") {
            functions.push_back(parse_function(declarations, declaration, true));
        } else if (head == "view-function") {
            const_view_functions.push_back(parse_function(declarations, declaration));
        } else if (head == "mutable-view-function") {
            mutable_view_functions.push_back(parse_function(declarations, declaration));
        }
    }

    std::optional<TypeRefId> equivalent_type;
    if (auto const value{fields.optional(P::equivalent_type)}) {
        equivalent_type = parse_type_ref(declarations, *value);
    }
    auto& records{declarations.soas};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "struct name");
    records.members[record_index] = declarations.range(members);
    records.export_specifier[record_index] = parse_export_specifier(fields);
    records.functions[record_index] = declarations.range(functions);
    records.mutable_view_functions[record_index] = declarations.range(mutable_view_functions);
    records.using_declarations[record_index] =
        declarations.range(text_id_list_or(fields, P::using_declarations));
    records.equivalent_type[record_index] = equivalent_type;
    records.equivalent_constructor[record_index] =
        optional_text_id(fields, P::equivalent_constructor);
    records.layout_only[record_index] = boolean_or(fields, P::layout_only);
    records.field_mask_name[record_index] = optional_identifier(fields, P::field_mask_name);
    records.field_enum_name[record_index] = optional_identifier(fields, P::field_enum_name);
    records.const_view_functions[record_index] = declarations.range(const_view_functions);
    return record_id;
}

auto SchemaParser::parse_module_settings(Declarations& declarations, sexpr::Fields const& fields)
    -> ModuleSettingsId {
    auto const source{optional_text_id(fields, P::source)};
    auto& records{declarations.module_settings_store};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "module name");
    records.header[record_index] = parser_.text_id(fields.required(P::header), "module header");
    records.source[record_index] = source;
    records.header_include[record_index] = optional_text_id(fields, P::header_include);
    records.namespace_name[record_index] = optional_identifier(fields, P::namespace_);
    records.include_order[record_index] =
        declarations.range(text_id_list_or(fields, P::include_order));
    records.prelude_lines[record_index] = declarations.range(cpp_lines_or(fields, P::prelude));
    return record_id;
}

auto SchemaParser::parse_native_enum_helper(sexpr::FormId const form) -> NativeEnumHelper {
    auto const value{parser_.text(form, "native enum helper")};
    if (auto const parsed{try_parse_serialized_native_enum_helper(value)}) {
        return *parsed;
    }
    parser_.fail(form, std::format("unknown native enum helper '{}'", value));
}

auto SchemaParser::parse_enum_unreal_projection(Declarations& declarations,
                                                sexpr::FormId const form)
    -> EnumUnrealProjectionId {
    Fields const fields{parser_, form, "unreal-projection", 1};
    fields.validate({P::header,
                     P::header_include,
                     P::conversion_header,
                     P::native_header_include,
                     P::reflection,
                     P::enum_array});

    auto reflection{EnumReflection::uenum};
    if (auto const value{fields.optional(P::reflection)}) {
        reflection = parse_keyword(
            parser_, *value, "enum reflection mode", try_parse_serialized_enum_reflection);
    }

    auto& records{declarations.enum_unreal_projections};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "Unreal projection name");
    records.header[record_index] =
        parser_.text_id(fields.required(P::header), "Unreal projection header");
    records.header_include[record_index] =
        parser_.text_id(fields.required(P::header_include), "Unreal projection header include");
    records.conversion_header[record_index] = parser_.text_id(
        fields.required(P::conversion_header), "Unreal projection conversion header");
    records.native_header_include[record_index] =
        parser_.text_id(fields.required(P::native_header_include), "native enum header include");
    records.reflection[record_index] = reflection;
    records.enum_array[record_index] = boolean_or(fields, P::enum_array);
    return record_id;
}

auto SchemaParser::parse_semantic_relation_unit(sexpr::FormId const form) -> SemanticRelationUnit {
    auto const value{parser_.text(form, "semantic relationship unit")};
    if (auto const parsed{try_parse_serialized_semantic_relation_unit(value)}) {
        return *parsed;
    }
    parser_.fail(form, "semantic relationship unit must be 'elements' or 'bytes'");
}

auto SchemaParser::parse_packed_byte_order(sexpr::FormId const form) -> PackedByteOrder {
    auto const value{parser_.text(form, "packed byte order")};
    if (auto const parsed{try_parse_serialized_packed_byte_order(value)}) {
        return *parsed;
    }
    parser_.fail(form, "packed byte order must be 'little' or 'big'");
}

auto SchemaParser::parse_packed_bit_order(sexpr::FormId const form) -> PackedBitOrder {
    auto const value{parser_.text(form, "packed bit order")};
    if (auto const parsed{try_parse_serialized_packed_bit_order(value)}) {
        return *parsed;
    }
    parser_.fail(form, "packed bit order must be 'lsb-first' or 'msb-first'");
}

auto SchemaParser::parse_packed_field(Declarations& declarations, sexpr::FormId const form)
    -> PackedSegmentId {
    Fields const fields{parser_, form, "field", 2};
    fields.validate({P::bits, P::kind, P::range_helper, P::minimum, P::maximum, P::default_},
                    {"code", "relation"});

    auto kind{PackedFieldKind::unsigned_integer};
    if (auto const value{fields.optional(P::kind)}) {
        kind = parse_keyword(
            parser_, *value, "packed field kind", try_parse_serialized_packed_field_kind);
    }

    auto const minimum{fields.optional(P::minimum)};
    auto const maximum{fields.optional(P::maximum)};
    auto const& bits_form{fields.required(P::bits)};
    auto const bits_text{std::string{parser_.text(bits_form, "packed field bits")}};
    auto const bits{bits_text == "auto"
                        ? std::optional<int>{}
                        : std::optional<int>{parser_.integer(bits_form, "packed field bits")}};
    std::vector<PackedNamedCodeId> named_codes;
    named_codes.reserve(fields.declarations().size());
    std::optional<SemanticRelationId> relationship;
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        if (syntax_.head(declaration) == "relation") {
            if (relationship.has_value()) {
                parser_.fail(declaration, "packed field may have only one relationship");
            }
            Fields const relation{parser_, declaration, "relation", 2};
            relation.validate({P::unit});
            auto const unit{relation.optional(P::unit)};
            {
                auto& records{declarations.semantic_relations};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.kind[record_index] =
                    parse_keyword(parser_,
                                  relation.positional(0),
                                  "semantic relationship kind",
                                  try_parse_serialized_semantic_relation_kind);
                records.target[record_index] = parse_type_ref(declarations, relation.positional(1));
                records.unit[record_index] = !unit.has_value()
                                               ? std::nullopt
                                               : std::optional{parse_semantic_relation_unit(*unit)};
                relationship = record_id;
            }
            continue;
        }
        Fields const code{parser_, declaration, "code", 1};
        code.validate({P::value, P::sentinel});
        {
            auto& records{declarations.packed_named_codes};
            auto const record_id{records.append()};
            auto const record_index{records.index(record_id)};
            records.name[record_index] =
                parser_.identifier(code.positional(0), "packed named code name");
            records.value[record_index] =
                packed_integer(code.required(P::value), "packed named code value");
            records.sentinel[record_index] = boolean_or(code, P::sentinel);
            named_codes.push_back(record_id);
        }
    }
    auto& records{declarations.packed_fields};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "packed field name");
    records.type[record_index] = parse_type_ref(declarations, fields.positional(1));
    records.bits[record_index] = bits;
    records.kind[record_index] = kind;
    records.range_helper[record_index] = boolean_or(fields, P::range_helper);
    records.minimum_value[record_index] =
        !minimum.has_value()
            ? std::nullopt
            : std::optional<PackedIntegerValue>{packed_integer(*minimum, "packed field minimum")};
    records.maximum_value[record_index] =
        !maximum.has_value()
            ? std::nullopt
            : std::optional<PackedIntegerValue>{packed_integer(*maximum, "packed field maximum")};
    records.named_codes[record_index] = declarations.range(named_codes);
    records.relationship[record_index] = relationship;
    records.default_value[record_index] =
        !fields.optional(P::default_).has_value()
            ? std::nullopt
            : std::optional{packed_integer(*fields.optional(P::default_), "packed field default")};
    return record_id;
}

auto SchemaParser::parse_packed_reserved_bits(Declarations& declarations, sexpr::FormId const form)
    -> PackedSegmentId {
    Fields const fields{parser_, form, "reserved", 1};
    fields.validate({P::bits});
    auto& records{declarations.packed_reserved_bits};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] =
        parser_.identifier(fields.positional(0), "packed reserved-region name");
    records.bits[record_index] =
        parser_.integer(fields.required(P::bits), "packed reserved-region bits");
    return record_id;
}

auto SchemaParser::parse_packed_value(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "packed-value", 1};
    fields.validate({P::storage,
                     P::invalid_value,
                     P::export_specifier,
                     P::mutable_,
                     P::byte_order,
                     P::bit_order},
                    {"field", "reserved"});
    auto const invalid_value{fields.optional(P::invalid_value)};
    auto const byte_order{fields.optional(P::byte_order)};
    auto const bit_order{fields.optional(P::bit_order)};

    std::vector<PackedSegmentId> segments;
    segments.reserve(fields.declarations().size());
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        if (syntax_.head(declaration) == "field") {
            segments.emplace_back(parse_packed_field(declarations, declaration));
        } else {
            segments.emplace_back(parse_packed_reserved_bits(declarations, declaration));
        }
    }

    auto& records{declarations.packed_values};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "packed value name");
    records.storage_type[record_index] = parse_type_ref(declarations, fields.required(P::storage));
    records.segments[record_index] = declarations.range(segments);
    records.invalid_value[record_index] =
        !invalid_value.has_value() ? std::nullopt
                                   : std::optional<std::uint64_t>{
                                         unsigned_integer(*invalid_value, "packed invalid value")};
    records.export_specifier[record_index] = parse_export_specifier(fields);
    records.mutable_value[record_index] = boolean_or(fields, P::mutable_);
    records.byte_order[record_index] =
        !byte_order.has_value()
            ? std::nullopt
            : std::optional<PackedByteOrder>{parse_packed_byte_order(*byte_order)};
    records.bit_order[record_index] =
        !bit_order.has_value() ? std::nullopt
                               : std::optional<PackedBitOrder>{parse_packed_bit_order(*bit_order)};
    return record_id;
}

auto SchemaParser::parse_enum(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    auto const has_underlying_type{syntax_.children(form).size() > 2 &&
                                   syntax_.kind(syntax_.children(form)[2]) !=
                                       lispb::codegen::sexpr::TokenKind::keyword &&
                                   (!syntax_.is_list(syntax_.children(form)[2]) ||
                                    syntax_.head(syntax_.children(form)[2]) == "type-ref")};
    Fields const fields{parser_, form, "enum", has_underlying_type ? 2U : 1U};
    fields.validate({P::reflection,
                     P::enum_array,
                     P::count,
                     P::conversions,
                     P::export_specifier,
                     P::native_api,
                     P::helpers,
                     P::bit_width,
                     P::signed_},
                    {"value", "unreal-projection"});
    std::vector<EnumeratorId> values;
    std::optional<EnumUnrealProjectionId> unreal_projection;
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        if (syntax_.head(declaration) == "value") {
            Fields const value{parser_, declaration, "value", 1};
            value.validate({P::value, P::display_name, P::hidden, P::serialized_name, P::sentinel});
            {
                auto& records{declarations.enumerators};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.name[record_index] =
                    parser_.identifier(value.positional(0), "enumerator name");
                records.initializer[record_index] = optional_text_id(value, P::value);
                records.display_name[record_index] = optional_text_id(value, P::display_name);
                records.hidden[record_index] = boolean_or(value, P::hidden);
                records.serialized_name[record_index] = optional_text_id(value, P::serialized_name);
                records.sentinel[record_index] = boolean_or(value, P::sentinel);
                values.push_back(record_id);
            }
        } else {
            if (unreal_projection.has_value()) {
                parser_.fail(declaration, "enum may define only one Unreal projection");
            }
            unreal_projection = parse_enum_unreal_projection(declarations, declaration);
        }
    }
    std::vector<NativeEnumHelper> helpers;
    if (auto const list{fields.optional(P::helpers)}) {
        if (!syntax_.is_list(*list)) {
            parser_.fail(*list, "helpers must be a list");
        }
        if (!boolean_or(fields, P::native_api)) {
            parser_.fail(*list, "helpers requires :native-api true");
        }
        for (auto const& child : syntax_.children(*list)) {
            helpers.push_back(parse_native_enum_helper(child));
        }
    }
    std::vector<EnumConversion> conversions;
    if (auto const list{fields.optional(P::conversions)}) {
        if (!syntax_.is_list(*list)) {
            parser_.fail(*list, "conversions must be a list");
        }
        for (auto const& child : syntax_.children(*list)) {
            conversions.push_back(parse_keyword(
                parser_, child, "enum conversion", try_parse_serialized_enum_conversion));
        }
    }
    auto reflection{EnumReflection::none};
    if (auto const value{fields.optional(P::reflection)}) {
        reflection = parse_keyword(
            parser_, *value, "enum reflection mode", try_parse_serialized_enum_reflection);
    }
    std::optional<std::uint8_t> bit_width;
    if (auto const value{fields.optional(P::bit_width)}) {
        auto const parsed{unsigned_integer(*value, "enum bit width")};
        if (parsed == 0 || parsed > 64) {
            parser_.fail(*value, "enum bit width must be between 1 and 64 bits");
        }
        bit_width = static_cast<std::uint8_t>(parsed);
    }
    auto& records{declarations.enums};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "enum name");
    records.underlying_type[record_index] =
        has_underlying_type ? std::optional{parse_type_ref(declarations, fields.positional(1))}
                            : std::nullopt;
    records.values[record_index] = declarations.range(values);
    records.count[record_index] = optional_identifier(fields, P::count);
    records.conversions[record_index] = declarations.range(conversions);
    records.export_specifier[record_index] = parse_export_specifier(fields);
    records.unreal_projection[record_index] = unreal_projection;
    records.bit_width[record_index] = bit_width;
    records.reflection[record_index] = reflection;
    records.signedness[record_index] = schema::EnumSignedness::unspecified;
    if (auto const signedness{fields.optional(P::signed_)}) {
        records.signedness[record_index] = parser_.boolean(*signedness, "signed")
                                             ? schema::EnumSignedness::signed_
                                             : schema::EnumSignedness::unsigned_;
    }
    records.enum_array[record_index] = boolean_or(fields, P::enum_array);
    records.native_api[record_index] = boolean_or(fields, P::native_api);
    records.helpers[record_index] = declarations.range(helpers);
    return record_id;
}

auto SchemaParser::parse_table(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "table", 1};
    fields.validate({P::export_specifier}, {"row", "column", "group"});
    std::vector<StaticTableRowId> rows;
    std::vector<StaticTableColumnId> columns;
    std::vector<StaticTableGroupId> groups;
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        auto const head{syntax_.head(declaration)};
        if (head == "row") {
            Fields const row{parser_, declaration, "row", 1};
            row.validate({});
            {
                auto& records{declarations.static_table_rows};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.name[record_index] = parser_.identifier(row.positional(0), "row name");
                rows.push_back(record_id);
            }
        } else if (head == "column") {
            Fields const column{parser_, declaration, "column", 2};
            column.validate({});
            {
                auto& records{declarations.static_table_columns};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.name[record_index] =
                    parser_.identifier(column.positional(0), "column name");
                records.type[record_index] = parse_type_ref(declarations, column.positional(1));
                columns.push_back(record_id);
            }
        } else if (head == "group") {
            Fields const group{parser_, declaration, "group", 2};
            group.validate({P::columns});
            {
                auto& records{declarations.static_table_groups};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.name[record_index] = parser_.identifier(group.positional(0), "group name");
                records.type[record_index] = parse_type_ref(declarations, group.positional(1));
                records.columns[record_index] = declarations.range(
                    identifier_list(group.required(P::columns), "group columns"));
                groups.push_back(record_id);
            }
        }
    }
    auto& records{declarations.static_tables};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "table name");
    records.rows[record_index] = declarations.range(rows);
    records.columns[record_index] = declarations.range(columns);
    records.groups[record_index] = declarations.range(groups);
    records.export_specifier[record_index] = parse_export_specifier(fields);
    return record_id;
}

auto SchemaParser::parse_facade_target_storage(sexpr::FormId const form) -> bool {
    auto const value{std::string{parser_.text(form, "facade target storage")}};
    if (value == "pointer") {
        return false;
    }
    if (value == "reference") {
        return true;
    }
    parser_.fail(form, std::format("expected 'pointer' or 'reference'; got '{}'", value));
}

auto SchemaParser::parse_facade_access(sexpr::Fields const& fields, SchemaProperty const key)
    -> Access {
    auto const property{to_serialized_string(key)};
    auto const form{fields.optional(property)};
    if (!form) {
        return Access::public_;
    }
    auto const value{try_parse_serialized_access(parser_.text(*form, property))};
    if (value && *value != Access::protected_) {
        return *value;
    }
    parser_.fail(*form, "facade access must be public or private");
}

auto SchemaParser::parse_facade_friend_kind(sexpr::Fields const& fields) -> RecordKind {
    auto const form{fields.optional(P::friend_kind)};
    if (!form) {
        return RecordKind::class_;
    }
    auto const kind{parse_keyword(parser_, *form, "friend kind", try_parse_serialized_record_kind)};
    if (kind == RecordKind::union_) {
        parser_.fail(*form, "facade friend_kind must be class or struct");
    }
    return kind;
}

auto SchemaParser::parse_facade(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "facade", 3};
    fields.validate({P::validation,
                     P::validation_dependencies,
                     P::export_specifier,
                     P::bind_access,
                     P::method_access,
                     P::friends,
                     P::friend_kind,
                     P::definitions_in_source,
                     P::target_storage},
                    {"method"});
    std::vector<FacadeMethodId> methods;
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        Fields const method{parser_, declaration, "method", 2};
        method.validate({P::const_, P::noexcept_, P::target_name}, {"parameter"});
        std::vector<ParameterId> parameters;
        for (auto const parameter_id : method.declarations()) {
            auto const parameter{parameter_id};
            parameters.push_back(parse_parameter(declarations, parameter));
        }
        {
            auto& records{declarations.facade_methods};
            auto const record_id{records.append()};
            auto const record_index{records.index(record_id)};
            records.name[record_index] =
                parser_.identifier(method.positional(0), "facade method name");
            records.return_type[record_index] = parse_type_ref(declarations, method.positional(1));
            records.parameters[record_index] = declarations.range(parameters);
            records.is_const[record_index] = boolean_or(method, P::const_);
            records.is_noexcept[record_index] = boolean_or(method, P::noexcept_);
            records.target_name[record_index] = optional_identifier(method, P::target_name);
            methods.push_back(record_id);
        }
    }
    auto reference_target{false};
    if (auto const value{fields.optional(P::target_storage)}) {
        reference_target = parse_facade_target_storage(*value);
    }
    auto& records{declarations.facades};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "facade name");
    records.target_type[record_index] = parse_type_ref(declarations, fields.positional(1));
    records.target_member_name[record_index] =
        parser_.identifier(fields.positional(2), "facade target member name");
    records.methods[record_index] = declarations.range(methods);
    records.validation_lines[record_index] =
        declarations.range(cpp_lines_or(fields, P::validation));
    records.validation_dependencies[record_index] =
        declarations.range(identifier_list_or(fields, P::validation_dependencies));
    records.export_specifier[record_index] = parse_export_specifier(fields);
    records.bind_access[record_index] = parse_facade_access(fields, P::bind_access);
    records.method_access[record_index] = parse_facade_access(fields, P::method_access);
    records.friends[record_index] = declarations.range(text_id_list_or(fields, P::friends));
    records.friend_kind[record_index] = parse_facade_friend_kind(fields);
    records.definitions_in_source[record_index] = boolean_or(fields, P::definitions_in_source);
    records.reference_target[record_index] = reference_target;
    return record_id;
}

auto SchemaParser::parse_setting_device(sexpr::FormId const form) -> SettingDevice {
    auto const value{parser_.text(form, "setting device")};
    if (auto const parsed{try_parse_serialized_setting_device(value)}) {
        return *parsed;
    }
    parser_.fail(form, std::format("unknown setting device '{}'", value));
}

auto SchemaParser::parse_control(Declarations& declarations, sexpr::FormId const form)
    -> SettingControlId {
    Fields const fields{parser_, form, "control", 1};
    fields.validate(
        {P::options_provider, P::availability_provider, P::min, P::max, P::step, P::custom_row});
    auto const kind{parse_keyword(parser_,
                                  fields.positional(0),
                                  "setting control kind",
                                  try_parse_serialized_setting_control_kind)};
    auto optional_number = [&](SchemaProperty const key) -> std::optional<double> {
        auto const name{to_serialized_string(key)};
        auto const value{fields.optional(name)};
        return !value.has_value() ? std::nullopt
                                  : std::optional<double>{parser_.number(*value, name)};
    };
    auto& records{declarations.setting_controls};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.kind[record_index] = kind;
    records.options_provider[record_index] = optional_text_id(fields, P::options_provider);
    records.availability_provider[record_index] =
        optional_text_id(fields, P::availability_provider);
    records.minimum[record_index] = optional_number(P::min);
    records.maximum[record_index] = optional_number(P::max);
    records.step[record_index] = optional_number(P::step);
    records.custom_row[record_index] = optional_text_id(fields, P::custom_row);
    return record_id;
}

auto SchemaParser::parse_setting(Declarations& declarations, sexpr::FormId const form)
    -> SettingId {
    Fields const fields{parser_, form, "setting", 2};
    fields.validate({P::tooltip, P::category, P::value_type, P::backend, P::apply, P::device},
                    {"control"});
    if (fields.declarations().size() != 1) {
        parser_.fail(form, "setting requires exactly one control declaration");
    }
    auto& records{declarations.settings};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "setting name");
    records.label[record_index] = parser_.text_id(fields.positional(1), "setting label");
    records.tooltip[record_index] = optional_text_id(fields, P::tooltip);
    records.category[record_index] =
        parser_.identifier(fields.required(P::category), "setting category");
    records.value_type[record_index] = parse_type_ref(declarations, fields.required(P::value_type));
    records.backend[record_index] =
        parser_.identifier(fields.required(P::backend), "setting backend");
    records.apply_mode[record_index] = parse_keyword(parser_,
                                                     fields.required(P::apply),
                                                     "settings apply mode",
                                                     try_parse_serialized_setting_apply_mode);
    records.device[record_index] = fields.optional(P::device).has_value()
                                     ? parse_setting_device(*fields.optional(P::device))
                                     : SettingDevice::shared;
    records.control[record_index] = parse_control(declarations, fields.declarations().front());
    return record_id;
}

auto SchemaParser::parse_record_member(Declarations& declarations, sexpr::FormId const form)
    -> RecordMemberId {
    Fields const fields{parser_, form, "member", 2};
    fields.validate({P::count,
                     P::initializer,
                     P::s7_exclude,
                     P::s7_name,
                     P::required,
                     P::nonzero,
                     P::positive,
                     P::nonnegative,
                     P::finite,
                     P::nonempty,
                     P::minimum,
                     P::maximum},
                    {"relation"});
    auto const count{fields.optional(P::count)};
    std::optional<SemanticRelationId> relationship;
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        if (relationship.has_value()) {
            parser_.fail(declaration, "record member may have only one relationship");
        }
        Fields const relation{parser_, declaration, "relation", 2};
        relation.validate({P::unit});
        auto const unit{relation.optional(P::unit)};
        {
            auto& records{declarations.semantic_relations};
            auto const record_id{records.append()};
            auto const record_index{records.index(record_id)};
            records.kind[record_index] = parse_keyword(parser_,
                                                       relation.positional(0),
                                                       "semantic relationship kind",
                                                       try_parse_serialized_semantic_relation_kind);
            records.target[record_index] = parse_type_ref(declarations, relation.positional(1));
            records.unit[record_index] = !unit.has_value()
                                           ? std::nullopt
                                           : std::optional{parse_semantic_relation_unit(*unit)};
            relationship = record_id;
        }
    }
    auto& records{declarations.record_members};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "record member name");
    records.type[record_index] = parse_type_ref(declarations, fields.positional(1));
    records.count[record_index] =
        !count.has_value()
            ? std::nullopt
            : std::optional<std::uint64_t>{unsigned_integer(*count, "record member count")};
    records.relationship[record_index] = relationship;
    records.initializer[record_index] = optional_text_id(fields, P::initializer);
    auto& s7{records.s7[record_index]};
    s7.name = optional_text_id(fields, P::s7_name);
    s7.exclude = boolean_or(fields, P::s7_exclude);
    s7.required = boolean_or(fields, P::required);
    s7.nonzero = boolean_or(fields, P::nonzero);
    s7.positive = boolean_or(fields, P::positive);
    s7.nonnegative = boolean_or(fields, P::nonnegative);
    s7.finite = boolean_or(fields, P::finite);
    s7.nonempty = boolean_or(fields, P::nonempty);
    auto const bound = [&](SchemaProperty const key) -> std::optional<StringId> {
        auto const name{to_serialized_string(key)};
        auto const value{fields.optional(name)};
        if (!value) {
            return {};
        }
        auto const number{parser_.number(*value, name)};
        if (!std::isfinite(number)) {
            parser_.fail(*value, "S7 field bounds must be finite");
        }
        auto const text{parser_.text(*value, name)};
        std::string spelling;
        if (text.find_first_of(".eE") == std::string_view::npos) {
            auto const integer{packed_integer(*value, name)};
            if (integer.negative && !packed_integer_as_signed(integer)) {
                parser_.fail(*value, "S7 integer bound is below the signed 64-bit range");
            }
            spelling = format_packed_integer(integer);
        } else {
            spelling = std::format("{:.17e}", number);
        }
        return declarations.strings().intern(spelling);
    };
    s7.minimum = bound(P::minimum);
    s7.maximum = bound(P::maximum);
    if (s7.minimum && s7.maximum &&
        parser_.number(fields.required(P::minimum), "minimum") >
            parser_.number(fields.required(P::maximum), "maximum")) {
        parser_.fail(form, "S7 field minimum exceeds maximum");
    }
    return record_id;
}

auto SchemaParser::parse_record(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "record", 1};
    fields.validate({P::export_specifier,
                     P::constraint_policy,
                     P::comparison,
                     P::comparison_noexcept,
                     P::generate_visitor,
                     P::generate_s7_reader,
                     P::s7_tag,
                     P::s7_positional,
                     P::s7_constructor,
                     P::s7_reader_includes},
                    {"member", "function"});
    auto comparison{RecordComparison::none};
    if (auto const value{fields.optional(P::comparison)}) {
        comparison = parse_keyword(
            parser_, *value, "record comparison", try_parse_serialized_record_comparison);
        if (comparison == RecordComparison::none) {
            parser_.fail(*value, "expected equality or three-way");
        }
    }
    std::vector<FunctionSchemaId> functions;
    std::vector<RecordMemberId> members;
    members.reserve(fields.declarations().size());
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        if (syntax_.head(declaration) == "function") {
            functions.push_back(parse_function(declarations, declaration));
        } else {
            members.push_back(parse_record_member(declarations, declaration));
        }
    }
    auto& records{declarations.records};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "record name");
    records.members[record_index] = declarations.range(members);
    records.export_specifier[record_index] = parse_export_specifier(fields);
    records.comparison[record_index] = comparison;
    records.comparison_noexcept[record_index] = boolean_or(fields, P::comparison_noexcept);
    records.constraint_policy[record_index] = optional_identifier(fields, P::constraint_policy);
    records.generate_visitor[record_index] = boolean_or(fields, P::generate_visitor);
    if (boolean_or(fields, P::generate_s7_reader)) {
        records.s7_tag[record_index] = parser_.text_id(fields.required(P::s7_tag), "S7 record tag");
        records.s7_positional[record_index] = boolean_or(fields, P::s7_positional);
        records.s7_constructor[record_index] = boolean_or(fields, P::s7_constructor, true);
        records.s7_reader_includes[record_index] =
            declarations.range(text_id_list_or(fields, P::s7_reader_includes));
    } else if (fields.optional(P::s7_tag) || fields.optional(P::s7_reader_includes) ||
               fields.optional(P::s7_positional) || fields.optional(P::s7_constructor)) {
        parser_.fail(form, "S7 reader options require generate-s7-reader true");
    }
    records.functions[record_index] = declarations.range(functions);
    return record_id;
}

auto SchemaParser::parse_union_alternative(Declarations& declarations, sexpr::FormId const form)
    -> UnionAlternativeId {
    Fields const fields{parser_, form, "alternative", 2};
    fields.validate({P::count});
    auto const count{fields.optional(P::count)};
    auto& records{declarations.union_alternatives};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "union alternative name");
    records.type[record_index] = parse_type_ref(declarations, fields.positional(1));
    records.count[record_index] =
        !count.has_value()
            ? std::nullopt
            : std::optional<std::uint64_t>{unsigned_integer(*count, "union alternative count")};
    return record_id;
}

auto SchemaParser::parse_union(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "union", 1};
    fields.validate({P::export_specifier}, {"alternative"});
    std::vector<UnionAlternativeId> alternatives;
    alternatives.reserve(fields.declarations().size());
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        alternatives.push_back(parse_union_alternative(declarations, declaration));
    }
    auto& records{declarations.unions};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "union name");
    records.alternatives[record_index] = declarations.range(alternatives);
    records.export_specifier[record_index] = parse_export_specifier(fields);
    return record_id;
}

auto SchemaParser::parse_tagged_union_alternative(Declarations& declarations,
                                                  sexpr::FormId const form)
    -> TaggedUnionAlternativeId {
    Fields const fields{parser_, form, "alternative", 2};
    fields.validate({P::count, P::tag});
    auto const count{fields.optional(P::count)};
    auto& records{declarations.tagged_union_alternatives};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] =
        parser_.identifier(fields.positional(0), "tagged union alternative name");
    records.type[record_index] = parse_type_ref(declarations, fields.positional(1));
    records.count[record_index] =
        !count.has_value() ? std::nullopt
                           : std::optional<std::uint64_t>{
                                 unsigned_integer(*count, "tagged union alternative count")};
    records.tag[record_index] =
        parser_.identifier(fields.required(P::tag), "tagged union alternative tag");
    return record_id;
}

auto SchemaParser::parse_tagged_union(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "tagged-union", 1};
    fields.validate({P::discriminant, P::export_specifier}, {"alternative"});
    std::vector<TaggedUnionAlternativeId> alternatives;
    alternatives.reserve(fields.declarations().size());
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        alternatives.push_back(parse_tagged_union_alternative(declarations, declaration));
    }
    auto& records{declarations.tagged_unions};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "tagged union name");
    records.discriminant[record_index] =
        parse_type_ref(declarations, fields.required(P::discriminant));
    records.alternatives[record_index] = declarations.range(alternatives);
    records.export_specifier[record_index] = parse_export_specifier(fields);
    return record_id;
}

auto SchemaParser::parse_integer_scalar(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "integer-scalar", 1};
    fields.validate(
        {P::signed_, P::minimum, P::maximum, P::bit_width, P::cpp_emission, P::cpp_type},
        {"code", "relation"});

    std::optional<std::uint32_t> bit_width;
    if (auto const width{fields.optional(P::bit_width)}) {
        auto const width_text{std::string{parser_.text(*width, "integer scalar bit width")}};
        if (width_text != "auto") {
            auto const parsed{parser_.integer(*width, "integer scalar bit width")};
            if (parsed <= 0 || parsed > 64) {
                parser_.fail(*width, "integer scalar bit width must be in the range 1..64");
            }
            bit_width = static_cast<std::uint32_t>(parsed);
        }
    }

    auto cpp_emission{IntegerScalarCppEmission::none};
    if (auto const emission{fields.optional(P::cpp_emission)}) {
        cpp_emission = parse_keyword(parser_,
                                     *emission,
                                     "integer scalar C++ emission policy",
                                     try_parse_serialized_integer_scalar_cpp_emission);
    }
    auto const cpp_type{fields.optional(P::cpp_type)};

    std::vector<PackedNamedCodeId> named_codes;
    named_codes.reserve(fields.declarations().size());
    std::optional<SemanticRelationId> relationship;
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        if (syntax_.head(declaration) == "relation") {
            if (relationship.has_value()) {
                parser_.fail(declaration, "integer scalar may have only one relationship");
            }
            Fields const relation{parser_, declaration, "relation", 2};
            relation.validate({P::unit});
            auto const unit{relation.optional(P::unit)};
            {
                auto& records{declarations.semantic_relations};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.kind[record_index] =
                    parse_keyword(parser_,
                                  relation.positional(0),
                                  "semantic relationship kind",
                                  try_parse_serialized_semantic_relation_kind);
                records.target[record_index] = parse_type_ref(declarations, relation.positional(1));
                records.unit[record_index] = !unit.has_value()
                                               ? std::nullopt
                                               : std::optional{parse_semantic_relation_unit(*unit)};
                relationship = record_id;
            }
            continue;
        }
        Fields const code{parser_, declaration, "code", 1};
        code.validate({P::value, P::sentinel});
        {
            auto& records{declarations.packed_named_codes};
            auto const record_id{records.append()};
            auto const record_index{records.index(record_id)};
            records.name[record_index] =
                parser_.identifier(code.positional(0), "integer scalar code name");
            records.value[record_index] =
                packed_integer(code.required(P::value), "integer scalar code value");
            records.sentinel[record_index] = boolean_or(code, P::sentinel);
            named_codes.push_back(record_id);
        }
    }

    auto& records{declarations.integer_scalars};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "integer scalar name");
    records.signedness[record_index] =
        parser_.boolean(fields.required(P::signed_), "integer scalar signedness");
    records.minimum_value[record_index] =
        packed_integer(fields.required(P::minimum), "integer scalar minimum");
    records.maximum_value[record_index] =
        packed_integer(fields.required(P::maximum), "integer scalar maximum");
    records.bit_width[record_index] = bit_width;
    records.named_codes[record_index] = declarations.range(named_codes);
    records.relationship[record_index] = relationship;
    records.cpp_emission[record_index] = cpp_emission;
    records.cpp_type[record_index] = !cpp_type.has_value()
                                       ? std::nullopt
                                       : std::optional{parse_type_ref(declarations, *cpp_type)};
    return record_id;
}

auto SchemaParser::parse_linear_quantized(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "linear-quantized", 1};
    fields.validate({P::source, P::bits, P::reserved_codes, P::clipping});

    auto clipping{QuantizationClipping::reject};
    if (auto const value{fields.optional(P::clipping)}) {
        auto const parsed{try_parse_serialized_quantization_clipping(
            parser_.text(*value, "linear quantization clipping policy"))};
        if (!parsed) {
            parser_.fail(*value, "linear quantization clipping must be reject or clamp");
        }
        clipping = *parsed;
    }

    auto const bits{parser_.integer(fields.required(P::bits), "linear quantization bit width")};
    if (bits <= 0 || bits > 64) {
        parser_.fail(fields.required(P::bits),
                     "linear quantization bit width must be in the range 1..64");
    }
    auto const reserved{fields.optional(P::reserved_codes)};
    auto& records{declarations.linear_quantizeds};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] =
        parser_.identifier(fields.positional(0), "linear quantization name");
    records.source[record_index] = parse_type_ref(declarations, fields.required(P::source));
    records.bit_width[record_index] = static_cast<std::uint32_t>(bits);
    records.reserved_codes[record_index] =
        reserved.has_value()
            ? unsigned_integer(*reserved, "linear quantization reserved code count")
            : 0;
    records.clipping[record_index] = clipping;
    return record_id;
}

auto SchemaParser::parse_integer_varint(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "integer-varint", 1};
    fields.validate({P::source, P::encoding});

    auto const encoding{parse_keyword(parser_,
                                      fields.required(P::encoding),
                                      "integer varint encoding",
                                      try_parse_serialized_integer_varint_encoding)};

    auto& records{declarations.integer_varints};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "integer varint name");
    records.source[record_index] = parse_type_ref(declarations, fields.required(P::source));
    records.encoding[record_index] = encoding;
    return record_id;
}

auto SchemaParser::parse_fixed_point(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "fixed-point", 1};
    fields.validate(
        {P::signed_, P::total_bits, P::fractional_bits, P::rounding, P::minimum, P::maximum});

    auto const total_bits{
        parser_.integer(fields.required(P::total_bits), "fixed-point total width")};
    if (total_bits <= 0 || total_bits > 64) {
        parser_.fail(fields.required(P::total_bits),
                     "fixed-point total width must be in the range 1..64");
    }
    auto const fractional_bits{
        parser_.integer(fields.required(P::fractional_bits), "fixed-point fractional width")};
    if (fractional_bits < 0 || fractional_bits > 64) {
        parser_.fail(fields.required(P::fractional_bits),
                     "fixed-point fractional width must be in the range 0..64");
    }

    auto rounding{FixedPointRounding::nearest_even};
    if (auto const value{fields.optional(P::rounding)}) {
        auto const parsed{try_parse_serialized_fixed_point_rounding(
            parser_.text(*value, "fixed-point rounding policy"))};
        if (!parsed) {
            parser_.fail(*value, "fixed-point rounding must be nearest-even or toward-zero");
        }
        rounding = *parsed;
    }

    auto const minimum{fields.optional(P::minimum)};
    auto const maximum{fields.optional(P::maximum)};
    auto& records{declarations.fixed_points};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "fixed-point name");
    records.signedness[record_index] =
        parser_.boolean(fields.required(P::signed_), "fixed-point signedness");
    records.total_bits[record_index] = static_cast<std::uint32_t>(total_bits);
    records.fractional_bits[record_index] = static_cast<std::uint32_t>(fractional_bits);
    records.rounding[record_index] = rounding;
    records.minimum_value[record_index] =
        !minimum.has_value() ? std::nullopt
                             : std::optional{parser_.text_id(*minimum, "fixed-point minimum")};
    records.maximum_value[record_index] =
        !maximum.has_value() ? std::nullopt
                             : std::optional{parser_.text_id(*maximum, "fixed-point maximum")};
    return record_id;
}

auto SchemaParser::parse_mini_float(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "mini-float", 1};
    fields.validate({P::sign_bits, P::exponent_bits, P::significand_bits, P::bias});

    auto const sign_bits{
        bounded_integer(parser_, fields.required(P::sign_bits), "mini-float sign width", 0, 1)};
    auto const exponent_bits{bounded_integer(
        parser_, fields.required(P::exponent_bits), "mini-float exponent width", 2, 15)};
    auto const significand_bits{bounded_integer(
        parser_, fields.required(P::significand_bits), "mini-float significand width", 0, 62)};
    if (sign_bits + exponent_bits + significand_bits > 64) {
        parser_.fail(form, "mini-float total width must not exceed 64 bits");
    }
    auto const exponent_bias{bounded_integer(
        parser_, fields.required(P::bias), "mini-float exponent bias", -32'768, 32'767)};

    auto& records{declarations.mini_floats};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "mini-float name");
    records.sign_bits[record_index] = static_cast<std::uint32_t>(sign_bits);
    records.exponent_bits[record_index] = static_cast<std::uint32_t>(exponent_bits);
    records.significand_bits[record_index] = static_cast<std::uint32_t>(significand_bits);
    records.exponent_bias[record_index] = exponent_bias;
    return record_id;
}

auto SchemaParser::parse_optional_sentinel(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "optional-sentinel", 1};
    fields.validate({P::source, P::sentinel});

    auto& records{declarations.optional_sentinels};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] = parser_.identifier(fields.positional(0), "optional sentinel name");
    records.source[record_index] = parse_type_ref(declarations, fields.required(P::source));
    records.sentinel[record_index] =
        parser_.identifier(fields.required(P::sentinel), "optional sentinel code");
    return record_id;
}

auto SchemaParser::parse_optional_presence_bit(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    Fields const fields{parser_, form, "optional-presence-bit", 1};
    fields.validate({P::source});

    auto& records{declarations.optional_presence_bits};
    auto const record_id{records.append()};
    auto const record_index{records.index(record_id)};
    records.name[record_index] =
        parser_.identifier(fields.positional(0), "optional presence-bit name");
    records.source[record_index] = parse_type_ref(declarations, fields.required(P::source));
    return record_id;
}

auto SchemaParser::parse_normal_declaration(Declarations& declarations, sexpr::FormId const form)
    -> DeclarationHandle {
    auto const head{syntax_.head(form)};
    auto const kind{try_parse_serialized_declaration_kind(head)};
    if (kind) {
        switch (*kind) {
            case DeclarationKind::enumeration:
                return parse_enum(declarations, form);
            case DeclarationKind::integer_scalar:
                return parse_integer_scalar(declarations, form);
            case DeclarationKind::linear_quantized:
                return parse_linear_quantized(declarations, form);
            case DeclarationKind::integer_varint:
                return parse_integer_varint(declarations, form);
            case DeclarationKind::fixed_point:
                return parse_fixed_point(declarations, form);
            case DeclarationKind::mini_float:
                return parse_mini_float(declarations, form);
            case DeclarationKind::optional_sentinel:
                return parse_optional_sentinel(declarations, form);
            case DeclarationKind::optional_presence_bit:
                return parse_optional_presence_bit(declarations, form);
            case DeclarationKind::packed_value:
                return parse_packed_value(declarations, form);
            case DeclarationKind::record:
                return parse_record(declarations, form);
            case DeclarationKind::union_type:
                return parse_union(declarations, form);
            case DeclarationKind::tagged_union:
                return parse_tagged_union(declarations, form);
            case DeclarationKind::soa:
                return parse_soa(declarations, form);
            case DeclarationKind::static_table:
                return parse_table(declarations, form);
            case DeclarationKind::facade:
                return parse_facade(declarations, form);
        }
    }
    parser_.fail(form, std::format("unknown normal module declaration '{}'", head));
}

auto SchemaParser::parse_module(Declarations& declarations, sexpr::FormId const form)
    -> ModuleHandle {
    auto const head{syntax_.head(form)};
    if (head == "module") {
        Fields const fields{parser_, form, head, 1};
        fields.validate({P::header,
                         P::source,
                         P::header_include,
                         P::namespace_,
                         P::include_order,
                         P::prelude,
                         P::helper_namespace,
                         P::s7_constructors},
                        {"enum",
                         "integer-scalar",
                         "linear-quantized",
                         "integer-varint",
                         "fixed-point",
                         "mini-float",
                         "optional-sentinel",
                         "optional-presence-bit",
                         "packed-value",
                         "record",
                         "union",
                         "tagged-union",
                         "struct",
                         "layout",
                         "table",
                         "facade"});
        auto& modules{declarations.normal_modules};
        auto const module{modules.append()};
        auto const module_index{modules.index(module)};
        modules.settings[module_index] = parse_module_settings(declarations, fields);
        modules.s7_constructors[module_index] = boolean_or(fields, P::s7_constructors);
        modules.enum_helper_namespace[module_index] =
            optional_identifier(fields, P::helper_namespace);
        std::vector<DeclarationHandle> module_declarations;
        std::optional<DiagnosticId> first_error;
        for (auto const declaration_id : fields.declarations()) {
            try {
                auto const declaration{declaration_id};
                module_declarations.push_back(parse_normal_declaration(declarations, declaration));
            } catch (sexpr::SourceError const& error) {
                if (!first_error) {
                    first_error = error.diagnostic();
                }
            }
        }
        if (first_error) {
            throw sexpr::SourceError{
                syntax_.sources().diagnostics(), syntax_.sources(), *first_error};
        }

        modules.declarations[module_index] = declarations.range(module_declarations);
        return module;
    }
    if (head == "settings-module") {
        Fields const fields{parser_, form, head, 1};
        fields.validate({P::header,
                         P::source,
                         P::header_include,
                         P::namespace_,
                         P::include_order,
                         P::prelude,
                         P::api_name,
                         P::state_name,
                         P::export_specifier},
                        {"category", "setting"});
        std::vector<SettingCategoryId> categories;
        std::vector<SettingId> settings;
        std::optional<DiagnosticId> first_error;
        for (auto const declaration_id : fields.declarations()) {
            try {
                auto const declaration{declaration_id};
                if (syntax_.head(declaration) == "category") {
                    Fields const category{parser_, declaration, "category", 2};
                    category.validate({});
                    {
                        auto& records{declarations.setting_categories};
                        auto const record_id{records.append()};
                        auto const record_index{records.index(record_id)};
                        records.name[record_index] =
                            parser_.identifier(category.positional(0), "category name");
                        records.label[record_index] =
                            parser_.text_id(category.positional(1), "category label");
                        categories.push_back(record_id);
                    }
                } else {
                    settings.push_back(parse_setting(declarations, declaration));
                }
            } catch (sexpr::SourceError const& error) {
                if (!first_error) {
                    first_error = error.diagnostic();
                }
            }
        }
        if (first_error) {
            throw sexpr::SourceError{
                syntax_.sources().diagnostics(), syntax_.sources(), *first_error};
        }

        auto& records{declarations.settings_modules};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.settings[record_index] = parse_module_settings(declarations, fields);
        records.api_name[record_index] =
            parser_.identifier(fields.required(P::api_name), "settings API name");
        records.state_name[record_index] =
            parser_.identifier(fields.required(P::state_name), "settings state name");
        records.export_specifier[record_index] = parse_export_specifier(fields);
        records.categories[record_index] = declarations.range(categories);
        records.settings_list[record_index] = declarations.range(settings);
        return record_id;
    }
    if (head == "umbrella-module") {
        Fields const fields{parser_, form, head, 1};
        fields.validate({P::header,
                         P::source,
                         P::header_include,
                         P::namespace_,
                         P::include_order,
                         P::prelude,
                         P::headers});
        auto& records{declarations.umbrella_modules};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.settings[record_index] = parse_module_settings(declarations, fields);
        records.headers[record_index] =
            declarations.range(text_id_list(fields.required(P::headers), "umbrella headers"));
        return record_id;
    }
    parser_.fail(form, std::format("unknown module declaration '{}'", head));
}

auto SchemaParser::parse_external_integer(RegisteredTypes& registered, sexpr::FormId const form)
    -> ExternalScalarId {
    auto& declarations{registered.declarations};
    Fields const fields{parser_, form, "integer", 0};
    fields.validate({P::signed_, P::bit_width, P::minimum, P::maximum}, {"code"});
    auto const width{parser_.integer(fields.required(P::bit_width), "external integer bit width")};
    if (width < 1 || width > 64) {
        parser_.fail(form, "external integer bit width must be in the range 1..64");
    }
    auto const signedness{
        parser_.boolean(fields.required(P::signed_), "external integer signedness")};
    auto const magnitude{std::uint64_t{1} << (width - 1)};
    auto const maximum{signedness    ? magnitude - 1
                       : width == 64 ? (std::numeric_limits<std::uint64_t>::max)()
                                     : (std::uint64_t{1} << width) - 1};
    auto minimum_value{signedness ? PackedIntegerValue::from_parts(true, magnitude)
                                  : PackedIntegerValue{0}};
    auto maximum_value{PackedIntegerValue{maximum}};
    if (auto const minimum{fields.optional(P::minimum)}) {
        minimum_value = packed_integer(*minimum, "external integer minimum");
    }
    if (auto const maximum_form{fields.optional(P::maximum)}) {
        maximum_value = packed_integer(*maximum_form, "external integer maximum");
    }
    std::vector<PackedNamedCodeId> named_codes;
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        Fields const code{parser_, declaration, "code", 1};
        code.validate({P::value, P::sentinel});
        {
            auto& records{declarations.packed_named_codes};
            auto const record_id{records.append()};
            auto const record_index{records.index(record_id)};
            records.name[record_index] = parser_.identifier(code.positional(0), "code name");
            records.value[record_index] = packed_integer(code.required(P::value), "code value");
            records.sentinel[record_index] = boolean_or(code, P::sentinel);
            named_codes.push_back(record_id);
        }
    }
    return registered.add_integer(signedness,
                                  minimum_value,
                                  maximum_value,
                                  static_cast<std::uint8_t>(width),
                                  declarations.range(named_codes));
}

auto SchemaParser::parse_type_definition(sexpr::FormId const form, TypeRegistry& registry)
    -> std::pair<lispb::StringId, RegisteredTypeId> {
    Fields const fields{parser_, form, "type", 1};
    fields.validate({P::spelling, P::header, P::pass_by},
                    {"operation", "integer", "floating-point"});
    ExternalScalarId semantics;
    auto& cpp_types{registry.cpp_types};
    auto const spelling{std::string{parser_.text(fields.required(P::spelling), "type spelling")}};
    std::vector<TypeDependencyId> dependencies;
    if (auto const header{optional_text(fields, P::header)}) {
        dependencies.push_back(cpp_types.dependency(spelling, *header));
    }
    auto const type{cpp_types.add(spelling, dependencies)};
    if (auto const pass_by{fields.optional(P::pass_by)}) {
        cpp_types.set_parameter_passing(
            type,
            parse_keyword(
                parser_, *pass_by, "parameter passing", try_parse_serialized_parameter_passing));
    }
    for (auto const declaration_id : fields.declarations()) {
        auto const declaration{declaration_id};
        if (syntax_.head(declaration) != "operation") {
            if (semantics.kind() != ExternalScalarKind::none) {
                parser_.fail(declaration, "type may have only one scalar description");
            }
            if (syntax_.head(declaration) == "integer") {
                semantics = parse_external_integer(registry.registered, declaration);
            } else {
                Fields const floating{parser_, declaration, "floating-point", 0};
                floating.validate({P::format});
                semantics = registry.registered.add_floating(
                    parse_keyword(parser_,
                                  floating.required(P::format),
                                  "floating-point format",
                                  try_parse_serialized_floating_point_format));
            }
            continue;
        }
        Fields const operation{parser_, declaration, "operation", 2};
        operation.validate({P::pass_by});
        auto const kind{parse_keyword(parser_,
                                      operation.positional(0),
                                      "type operation",
                                      try_parse_serialized_type_operation)};
        if (cpp_types.operation(type, kind).has_value()) {
            parser_.fail(declaration,
                         std::format("duplicate type operation '{}'", to_serialized_string(kind)));
        }
        auto passing{ParameterPassing::const_reference};
        if (kind != TypeOperation::remove_at_swap) {
            auto const pass_by{operation.optional(P::pass_by)};
            if (pass_by.has_value()) {
                passing = parse_keyword(
                    parser_, *pass_by, "parameter passing", try_parse_serialized_parameter_passing);
            }
        } else if (operation.optional(P::pass_by).has_value()) {
            parser_.fail(*operation.optional(P::pass_by),
                         "remove-at-swap does not accept ':pass-by'");
        }
        cpp_types.set_operation(
            type,
            kind,
            std::string{parser_.text(operation.positional(1), "type operation function")},
            passing);
    }
    return {parser_.identifier(fields.positional(0), "type name"),
            registry.registered.append(type, semantics)};
}

}
