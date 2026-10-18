#include <codegen/source_loader.h>

#include <codegen/ast/access.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/parameter_passing.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/type_operation.h>
#include <codegen/loaded_module_source.h>
#include <codegen/loaded_type_registry.h>
#include <codegen/manifest_error.h>
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
#include <codegen/schema/manifest.h>
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
#include <codegen/schema/schema_version.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_unit.h>
#include <codegen/schema/setting_apply_mode.h>
#include <codegen/schema/setting_control_kind.h>
#include <codegen/schema/setting_device.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_storage.h>
#include <codegen/schema/storage_operation.h>
#include <codegen/schema/type_registry.h>
#include <codegen/sexpr/fields.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/token_kind.h>
#include <lispb/compiler_storage.h>
#include <lispb/indices.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {
namespace detail {
namespace {
thread_local ModuleSourceReadHook module_source_read_hook{};
}

auto set_module_source_read_hook_for_testing(ModuleSourceReadHook const hook) noexcept
    -> ModuleSourceReadHook {
    return std::exchange(module_source_read_hook, hook);
}

} // namespace detail

namespace {

using sexpr::FormId;
using sexpr::SourceSpan;
using sexpr::Syntax;

[[noreturn]] void
    fail_at(sexpr::Sources const& sources, SourceSpan const& span, std::string const& message) {
    auto const location{sources.location(span)};
    throw ManifestError{std::string{location.path} + ":" + std::to_string(location.line) + ":" +
                        std::to_string(location.column) + ": error: " + message};
}

class Fields : public sexpr::Fields {
  public:
    Fields(Syntax const& syntax,
           FormId const form,
           std::string_view const expected_head,
           std::size_t const positionals)
        : sexpr::Fields{syntax, form, expected_head, positionals, fail_at} {}
};

class SourceParser {
  public:
    explicit SourceParser(Syntax& syntax)
        : syntax_{syntax} {}

    [[noreturn]] void fail(FormId const form, std::string const& message) {
        fail_at(syntax_.sources(), syntax_.token_span(form), message);
    }

    auto text(FormId const form, std::string_view const purpose) -> std::string {
        return std::string{sexpr::text(syntax_, form, purpose, fail_at)};
    }

    auto text_id(FormId const form, std::string_view const purpose) -> lispb::StringId {
        if (syntax_.is_list(form) || (syntax_.kind(form) != sexpr::TokenKind::atom &&
                                      syntax_.kind(form) != sexpr::TokenKind::string)) {
            fail(form, std::string{purpose} + " must be text or a symbol");
        }
        return syntax_.text_id(form);
    }

    auto identifier(FormId const form, std::string_view const purpose) -> lispb::IdentifierId {
        auto const text{text_id(form, purpose)};
        return syntax_.kind(form) == sexpr::TokenKind::atom ? syntax_.identifier(form)
                                                            : syntax_.strings().intern(text);
    }

    auto optional_text_id(Fields const& fields, std::string_view const name)
        -> std::optional<lispb::StringId> {
        auto const value{fields.optional(name)};
        return value ? std::optional{text_id(*value, name)} : std::nullopt;
    }

    auto parse_export_specifier(Fields const& fields) -> lispb::IdentifierId {
        auto const value{fields.optional("export-specifier")};
        if (!value) {
            return {};
        }
        auto const result{identifier(*value, "export specifier")};
        if (result.value == 0) {
            fail(*value, "export specifier cannot be empty");
        }
        return result;
    }

    auto optional_identifier(Fields const& fields, std::string_view const name)
        -> std::optional<lispb::IdentifierId> {
        auto const value{fields.optional(name)};
        return value ? std::optional{identifier(*value, name)} : std::nullopt;
    }

    auto text_id_list(FormId const form, std::string_view const purpose)
        -> std::vector<lispb::StringId> {
        if (!syntax_.is_list(form)) {
            fail(form, std::string{purpose} + " must be a list");
        }
        std::vector<lispb::StringId> result;
        result.reserve(syntax_.children(form).size());
        for (auto const child : syntax_.children(form)) {
            result.push_back(text_id(child, purpose));
        }
        return result;
    }

    auto identifier_list(FormId const form, std::string_view const purpose)
        -> std::vector<lispb::IdentifierId> {
        if (!syntax_.is_list(form)) {
            fail(form, std::string{purpose} + " must be a list");
        }
        std::vector<lispb::IdentifierId> result;
        result.reserve(syntax_.children(form).size());
        for (auto const child : syntax_.children(form)) {
            result.push_back(identifier(child, purpose));
        }
        return result;
    }

    auto text_id_list_or(Fields const& fields, std::string_view const name)
        -> std::vector<lispb::StringId> {
        auto const value{fields.optional(name)};
        return value ? text_id_list(*value, name) : std::vector<lispb::StringId>{};
    }

    auto identifier_list_or(Fields const& fields, std::string_view const name)
        -> std::vector<lispb::IdentifierId> {
        auto const value{fields.optional(name)};
        return value ? identifier_list(*value, name) : std::vector<lispb::IdentifierId>{};
    }

    auto boolean(FormId const form, std::string_view const purpose) -> bool {
        return sexpr::boolean(syntax_, form, purpose, fail_at);
    }

    auto number(FormId const form, std::string_view const purpose) -> double {
        return sexpr::number(syntax_, form, purpose, fail_at);
    }

    auto integer(FormId const form, std::string_view const purpose) -> int {
        return sexpr::integer(syntax_, form, purpose, fail_at);
    }

    auto unsigned_integer(FormId const form, std::string_view const purpose) -> std::uint64_t {
        auto const value{text(form, purpose)};
        if (value.empty() || value.front() == '-') {
            fail(form, std::string{purpose} + " must be an unsigned integer");
        }
        try {
            std::size_t parsed{};
            auto const result{std::stoull(value, &parsed, 0)};
            if (parsed != value.size()) {
                fail(form, std::string{purpose} + " must be an unsigned integer");
            }
            return result;
        } catch (std::invalid_argument const&) {
            fail(form, std::string{purpose} + " must be an unsigned integer");
        } catch (std::out_of_range const&) {
            fail(form, std::string{purpose} + " is out of range");
        }
    }

    auto packed_integer(FormId const form, std::string_view const purpose) -> PackedIntegerValue {
        auto value{text(form, purpose)};
        auto negative{false};
        if (!value.empty() && (value.front() == '-' || value.front() == '+')) {
            negative = value.front() == '-';
            value.erase(value.begin());
        }
        if (value.empty()) {
            fail(form, std::string{purpose} + " must be an integer");
        }
        try {
            std::size_t parsed{};
            auto const magnitude{std::stoull(value, &parsed, 0)};
            if (parsed != value.size()) {
                fail(form, std::string{purpose} + " must be an integer");
            }
            return PackedIntegerValue::from_parts(negative, magnitude);
        } catch (std::invalid_argument const&) {
            fail(form, std::string{purpose} + " must be an integer");
        } catch (std::out_of_range const&) {
            fail(form, std::string{purpose} + " is out of range");
        }
    }

    auto optional_text(Fields const& fields, std::string_view const name)
        -> std::optional<std::string> {
        auto const value{fields.optional(name)};
        return !value.has_value() ? std::nullopt : std::optional<std::string>{text(*value, name)};
    }

    auto cpp_lines_or(Fields const& fields, std::string_view const name)
        -> std::vector<lispb::StringId> {
        auto const value{fields.optional(name)};
        if (!value.has_value()) {
            return {};
        }
        if (syntax_.kind(*value) != sexpr::TokenKind::raw_literal) {
            return text_id_list(*value, name);
        }

        if (syntax_.tag(*value) != "cpp") {
            fail(*value,
                 std::string{name} + " requires a #cpp raw literal; got #" +
                     std::string{syntax_.tag(*value)});
        }
        if (syntax_.text(*value).empty()) {
            return {};
        }
        return {syntax_.text_id(*value)};
    }

    auto boolean_or(Fields const& fields, std::string_view const name, bool const fallback = false)
        -> bool {
        auto const value{fields.optional(name)};
        return !value.has_value() ? fallback : boolean(*value, name);
    }

    auto parse_type_ref(Declarations& declarations, FormId const form) -> TypeRefId {
        if (!syntax_.is_list(form)) {
            auto& records{declarations.type_refs};
            auto const record_id{records.append()};
            auto const record_index{records.index(record_id)};
            records.name[record_index] = identifier(form, "type reference");
            records.suffix[record_index] = {};
            records.nested[record_index] = std::nullopt;
            return record_id;
        }
        Fields const fields{syntax_, form, "type-ref", 1};
        fields.validate({"suffix", "nested"});
        auto& records{declarations.type_refs};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "type reference name");
        records.suffix[record_index] =
            optional_text_id(fields, "suffix").value_or(lispb::StringId{});
        records.nested[record_index] = optional_text_id(fields, "nested");
        return record_id;
    }

    auto type_ref_list(Declarations& declarations,
                       FormId const form,
                       std::string_view const purpose) -> std::vector<TypeRefId> {
        if (!syntax_.is_list(form)) {
            fail(form, std::string{purpose} + " must be a list");
        }
        std::vector<TypeRefId> result;
        result.reserve(syntax_.children(form).size());
        for (auto const& child : syntax_.children(form)) {
            result.push_back(parse_type_ref(declarations, child));
        }
        return result;
    }

    auto parse_parameter_passing(FormId const form) -> ParameterPassing {
        auto const value{text(form, "parameter passing")};
        if (value == "const-ref") {
            return ParameterPassing::const_reference;
        }
        if (value == "value") {
            return ParameterPassing::value;
        }
        fail(form, "expected 'const-ref' or 'value'; got '" + value + "'");
    }

    auto parse_parameter(Declarations& declarations, FormId const form) -> ParameterId {
        Fields const fields{syntax_, form, "parameter", 2};
        fields.validate({"default"});
        auto& records{declarations.parameters};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.type[record_index] = parse_type_ref(declarations, fields.positional(1));
        records.name[record_index] = identifier(fields.positional(0), "parameter name");
        records.default_value[record_index] = optional_text_id(fields, "default");
        return record_id;
    }

    auto parse_function(Declarations& declarations, FormId const form) -> FunctionSchemaId {
        Fields const fields{syntax_, form, syntax_.head(form), 2};
        fields.validate({"body",
                         "dependencies",
                         "trailing-return-type",
                         "const",
                         "noexcept",
                         "static",
                         "inline",
                         "constexpr",
                         "nodiscard",
                         "definition-in-source",
                         "template-parameters",
                         "requires"},
                        {"parameter"});
        std::vector<ParameterId> parameters;
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            parameters.push_back(parse_parameter(declarations, declaration));
        }
        std::optional<TypeRefId> trailing_return_type;
        if (auto const value{fields.optional("trailing-return-type")}) {
            trailing_return_type = parse_type_ref(declarations, *value);
        }
        auto const is_static{boolean_or(fields, "static")};
        auto const is_const{
            boolean_or(fields, "const", syntax_.head(form) == "view-function" && !is_static)};
        if (syntax_.head(form) == "view-function" && !is_static && !is_const) {
            fail(form,
                 "view-function must be const; use mutable-view-function for a mutable receiver");
        }
        auto body_lines{cpp_lines_or(fields, "body")};
        if (fields.optional("body").has_value() && body_lines.empty()) {
            body_lines.emplace_back();
        }
        auto& records{declarations.functions};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "function name");
        records.return_type[record_index] = parse_type_ref(declarations, fields.positional(1));
        records.parameters[record_index] = declarations.range(parameters);
        records.body_lines[record_index] = declarations.range(body_lines);
        records.dependencies[record_index] =
            declarations.range(identifier_list_or(fields, "dependencies"));
        records.trailing_return_type[record_index] = std::move(trailing_return_type);
        records.is_const[record_index] = is_const;
        records.is_noexcept[record_index] = boolean_or(fields, "noexcept");
        records.is_static[record_index] = is_static;
        records.is_inline[record_index] = boolean_or(fields, "inline");
        records.definition_in_source[record_index] = boolean_or(fields, "definition-in-source");
        records.template_parameters[record_index] = optional_text_id(fields, "template-parameters");
        records.requires_clause[record_index] = optional_text_id(fields, "requires");
        records.is_constexpr[record_index] = boolean_or(fields, "constexpr");
        records.is_nodiscard[record_index] = boolean_or(fields, "nodiscard");
        return record_id;
    }

    auto parse_operation(FormId const form) -> StorageOperation {
        auto const value{text(form, "storage operation")};
        static constexpr std::pair<std::string_view, StorageOperation> operations[]{
            {"reset", StorageOperation::reset},
            {"reserve", StorageOperation::reserve},
            {"add-uninitialised", StorageOperation::add_uninitialised},
            {"add-defaulted", StorageOperation::add_defaulted},
            {"remove-at-swap", StorageOperation::remove_at_swap},
            {"set-num", StorageOperation::set_num},
            {"copy-element", StorageOperation::copy_element},
            {"append-from", StorageOperation::append_from},
        };
        auto const found{std::ranges::find(
            operations, value, &std::pair<std::string_view, StorageOperation>::first)};
        if (found == std::end(operations)) {
            fail(form, "unknown storage operation '" + value + "'");
        }
        return found->second;
    }

    auto parse_operations(FormId const form) -> std::vector<StorageOperation> {
        if (!syntax_.is_list(form)) {
            fail(form, "operations must be a list");
        }
        if (syntax_.children(form).size() == 1 &&
            text(syntax_.children(form).front(), "storage operation") == "all") {
            return all_storage_operations();
        }
        std::vector<StorageOperation> result;
        result.reserve(syntax_.children(form).size());
        for (auto const& child : syntax_.children(form)) {
            if (text(child, "storage operation") == "all") {
                fail(child, "'all' must be the only storage operation");
            }
            result.push_back(parse_operation(child));
        }
        return result;
    }

    auto parse_fixed(Declarations& declarations, FormId const form) -> FixedSoaId {
        Fields const fields{syntax_, form, "fixed", 1};
        fields.validate({"containers"});
        auto& records{declarations.fixed_soas};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.storage_name[record_index] = identifier(fields.positional(0), "fixed storage name");
        records.containers[record_index] =
            declarations.range(identifier_list_or(fields, "containers"));
        return record_id;
    }

    auto parse_member(Declarations& declarations, FormId const form) -> SoaMemberId {
        Fields const fields{syntax_, form, "member", 3};
        fields.validate({"fixed-schema", "nested-schema", "mask-field", "mask-dimensions"},
                        {"relation"});
        auto const kind_name{text(fields.positional(1), "member kind")};
        SoaMemberKind kind;
        if (kind_name == "array") {
            kind = SoaMemberKind::array;
        } else if (kind_name == "nested") {
            kind = SoaMemberKind::nested;
        } else {
            fail(fields.positional(1), "unknown SOA member kind '" + kind_name + "'");
        }
        std::vector<SoaMaskDimensionId> mask_dimensions;
        if (auto const dimensions{fields.optional("mask-dimensions")}) {
            if (!syntax_.is_list(*dimensions)) {
                fail(*dimensions, "mask-dimensions must be a list");
            }
            for (auto const& dimension : syntax_.children(*dimensions)) {
                if (!syntax_.is_list(dimension) || syntax_.children(dimension).size() != 2) {
                    fail(dimension,
                         "mask dimension must contain an index name and extent expression");
                }
                {
                    auto& records{declarations.soa_mask_dimensions};
                    auto const record_id{records.append()};
                    auto const record_index{records.index(record_id)};
                    records.index_name[record_index] =
                        identifier(syntax_.children(dimension)[0], "mask dimension index name");
                    records.extent[record_index] =
                        text_id(syntax_.children(dimension)[1], "mask dimension extent");
                    mask_dimensions.push_back(record_id);
                }
            }
        }
        std::optional<SemanticRelationId> relationship;
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            if (relationship.has_value()) {
                fail(declaration, "SOA member may have only one relationship");
            }
            Fields const relation{syntax_, declaration, "relation", 2};
            relation.validate({"unit"});
            auto const unit{relation.optional("unit")};
            {
                auto& records{declarations.semantic_relations};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.kind[record_index] = parse_semantic_relation_kind(relation.positional(0));
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
        records.name[record_index] = identifier(fields.positional(0), "member name");
        records.kind[record_index] = kind;
        records.type[record_index] = parse_type_ref(declarations, fields.positional(2));
        records.fixed_schema[record_index] = optional_identifier(fields, "fixed-schema");
        records.nested_schema[record_index] = optional_identifier(fields, "nested-schema");
        records.mask_field[record_index] = boolean_or(fields, "mask-field");
        records.mask_dimensions[record_index] = declarations.range(mask_dimensions);
        records.relationship[record_index] = std::move(relationship);
        return record_id;
    }

    auto parse_single_allocation(FormId const form) -> lispb::IdentifierId {
        Fields const fields{syntax_, form, "single-allocation", 1};
        fields.validate({});
        return identifier(fields.positional(0), "single-allocation name");
    }

    auto parse_soa(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "struct", 1};
        fields.validate({"storage",
                         "view-name",
                         "const-view-name",
                         "operations",
                         "export-specifier",
                         "using-declarations",
                         "array-allocator",
                         "equivalent-type",
                         "vector-components",
                         "copy-element-memberwise",
                         "layout-only",
                         "field-mask-name",
                         "field-enum-name"},
                        {"member",
                         "function",
                         "view-function",
                         "mutable-view-function",
                         "fixed",
                         "single-allocation"});

        std::vector<SoaMemberId> members;
        std::vector<FunctionSchemaId> functions;
        std::vector<FunctionSchemaId> const_view_functions;
        std::vector<FunctionSchemaId> mutable_view_functions;
        std::optional<FixedSoaId> fixed;
        std::optional<lispb::IdentifierId> single_allocation;
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            auto const head{syntax_.head(declaration)};
            if (head == "member") {
                members.push_back(parse_member(declarations, declaration));
            } else if (head == "function") {
                functions.push_back(parse_function(declarations, declaration));
            } else if (head == "view-function") {
                const_view_functions.push_back(parse_function(declarations, declaration));
            } else if (head == "mutable-view-function") {
                mutable_view_functions.push_back(parse_function(declarations, declaration));
            } else if (head == "fixed") {
                if (fixed.has_value()) {
                    fail(declaration, "duplicate 'fixed' declaration");
                }
                fixed = parse_fixed(declarations, declaration);
            } else if (head == "single-allocation") {
                if (single_allocation.has_value()) {
                    fail(declaration, "duplicate 'single-allocation' declaration");
                }
                single_allocation = parse_single_allocation(declaration);
            }
        }

        std::vector<StorageOperation> operations;
        if (auto const value{fields.optional("operations")}) {
            operations = parse_operations(*value);
        }
        std::optional<TypeRefId> equivalent_type;
        if (auto const value{fields.optional("equivalent-type")}) {
            equivalent_type = parse_type_ref(declarations, *value);
        }
        std::optional<SoaStorage> storage;
        if (auto const value{optional_text(fields, "storage")}) {
            if (*value == "vector") {
                storage = SoaStorage::vector;
            } else if (*value == "single-allocation") {
                storage = SoaStorage::single_allocation;
            } else if (*value == "both") {
                storage = SoaStorage::both;
            } else {
                fail(form, "storage must be vector, single-allocation, or both");
            }
        }
        auto& records{declarations.soas};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "struct name");
        records.view_name[record_index] = optional_identifier(fields, "view-name");
        records.const_view_name[record_index] = optional_identifier(fields, "const-view-name");
        records.members[record_index] = declarations.range(members);
        records.operations[record_index] = declarations.range(operations);
        records.export_specifier[record_index] = parse_export_specifier(fields);
        records.functions[record_index] = declarations.range(functions);
        records.mutable_view_functions[record_index] = declarations.range(mutable_view_functions);
        records.using_declarations[record_index] =
            declarations.range(text_id_list_or(fields, "using-declarations"));
        records.equivalent_type[record_index] = std::move(equivalent_type);
        records.copy_element_memberwise[record_index] =
            boolean_or(fields, "copy-element-memberwise");
        records.layout_only[record_index] = boolean_or(fields, "layout-only");
        records.fixed[record_index] = std::move(fixed);
        records.single_allocation[record_index] = std::move(single_allocation);
        records.array_allocator[record_index] =
            fields.optional("array-allocator")
                ? std::optional{parse_type_ref(declarations, *fields.optional("array-allocator"))}
                : std::nullopt;
        records.field_mask_name[record_index] = optional_identifier(fields, "field-mask-name");
        records.field_enum_name[record_index] = optional_identifier(fields, "field-enum-name");
        records.vector_components[record_index] =
            declarations.range(identifier_list_or(fields, "vector-components"));
        records.storage[record_index] = storage;
        records.const_view_functions[record_index] = declarations.range(const_view_functions);
        return record_id;
    }

    auto parse_module_settings(Declarations& declarations, Fields const& fields)
        -> ModuleSettingsId {
        auto const source{optional_text_id(fields, "source")};
        auto& records{declarations.module_settings_store};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "module name");
        records.header[record_index] = text_id(fields.required("header"), "module header");
        records.source[record_index] = std::move(source);
        records.header_include[record_index] = optional_text_id(fields, "header-include");
        records.namespace_name[record_index] = optional_identifier(fields, "namespace");
        records.include_order[record_index] =
            declarations.range(text_id_list_or(fields, "include-order"));
        records.prelude_lines[record_index] = declarations.range(cpp_lines_or(fields, "prelude"));
        return record_id;
    }

    auto parse_enum_reflection(FormId const form) -> EnumReflection {
        auto const value{text(form, "enum reflection")};
        if (value == "none") {
            return EnumReflection::none;
        }
        if (value == "uenum") {
            return EnumReflection::uenum;
        }
        if (value == "blueprint") {
            return EnumReflection::blueprint;
        }
        fail(form, "unknown enum reflection mode '" + value + "'");
    }

    auto parse_enum_conversion(FormId const form) -> EnumConversion {
        auto const value{text(form, "enum conversion")};
        static constexpr std::pair<std::string_view, EnumConversion> conversions[]{
            {"lex-to-string", EnumConversion::lex_to_string},
            {"string-view", EnumConversion::string_view},
            {"string", EnumConversion::string},
            {"lex-to-display-string", EnumConversion::lex_to_display_string},
            {"display-string-view", EnumConversion::display_string_view},
            {"display-string", EnumConversion::display_string},
            {"lex-to-serialized-string", EnumConversion::lex_to_serialized_string},
            {"try-parse-serialized", EnumConversion::try_parse_serialized},
        };
        auto const found{std::ranges::find(
            conversions, value, &std::pair<std::string_view, EnumConversion>::first)};
        if (found == std::end(conversions)) {
            fail(form, "unknown enum conversion '" + value + "'");
        }
        return found->second;
    }

    auto parse_native_enum_helper(FormId const form) -> NativeEnumHelper {
        auto const value{text(form, "native enum helper")};
        constexpr std::array helpers{
            NativeEnumHelper::to_string,
            NativeEnumHelper::try_parse,
            NativeEnumHelper::to_display_string,
            NativeEnumHelper::to_serialized_string,
            NativeEnumHelper::try_parse_serialized,
            NativeEnumHelper::traits,
        };
        for (auto const helper : helpers) {
            if (native_enum_helper_name(helper) == value) {
                return helper;
            }
        }
        fail(form, "unknown native enum helper '" + value + "'");
    }

    auto parse_enum_unreal_projection(Declarations& declarations, FormId const form)
        -> EnumUnrealProjectionId {
        Fields const fields{syntax_, form, "unreal-projection", 1};
        fields.validate({"header",
                         "header-include",
                         "conversion-header",
                         "native-header-include",
                         "reflection",
                         "enum-array"});

        auto reflection{EnumReflection::uenum};
        if (auto const value{fields.optional("reflection")}) {
            reflection = parse_enum_reflection(*value);
        }

        auto& records{declarations.enum_unreal_projections};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "Unreal projection name");
        records.header[record_index] =
            text_id(fields.required("header"), "Unreal projection header");
        records.header_include[record_index] =
            text_id(fields.required("header-include"), "Unreal projection header include");
        records.conversion_header[record_index] =
            text_id(fields.required("conversion-header"), "Unreal projection conversion header");
        records.native_header_include[record_index] =
            text_id(fields.required("native-header-include"), "native enum header include");
        records.reflection[record_index] = reflection;
        records.enum_array[record_index] = boolean_or(fields, "enum-array");
        return record_id;
    }

    auto parse_semantic_relation_kind(FormId const form) -> SemanticRelationKind {
        auto const name{text(form, "semantic relationship kind")};
        constexpr std::array kinds{
            std::pair{"index_into", SemanticRelationKind::index_into},
            std::pair{"count_of", SemanticRelationKind::count_of},
            std::pair{"offset_into", SemanticRelationKind::offset_into},
            std::pair{"discriminates", SemanticRelationKind::discriminates},
            std::pair{"contains", SemanticRelationKind::contains},
            std::pair{"member_of", SemanticRelationKind::member_of},
            std::pair{"quantises", SemanticRelationKind::quantises},
            std::pair{"encoded_as", SemanticRelationKind::encoded_as},
            std::pair{"references", SemanticRelationKind::references},
        };
        auto const found{std::ranges::find_if(
            kinds, [&](auto const& candidate) { return candidate.first == name; })};
        if (found == kinds.end()) {
            fail(form, "unknown semantic relationship kind: " + name);
        }
        return found->second;
    }

    auto parse_semantic_relation_unit(FormId const form) -> SemanticRelationUnit {
        auto const name{text(form, "semantic relationship unit")};
        if (name == "elements") {
            return SemanticRelationUnit::elements;
        }
        if (name == "bytes") {
            return SemanticRelationUnit::bytes;
        }
        fail(form, "semantic relationship unit must be 'elements' or 'bytes'");
    }

    auto parse_packed_byte_order(FormId const form) -> PackedByteOrder {
        auto const name{text(form, "packed byte order")};
        if (name == "little") {
            return PackedByteOrder::little_endian;
        }
        if (name == "big") {
            return PackedByteOrder::big_endian;
        }
        fail(form, "packed byte order must be 'little' or 'big'");
    }

    auto parse_packed_bit_order(FormId const form) -> PackedBitOrder {
        auto const name{text(form, "packed bit order")};
        if (name == "lsb-first") {
            return PackedBitOrder::least_significant_first;
        }
        if (name == "msb-first") {
            return PackedBitOrder::most_significant_first;
        }
        fail(form, "packed bit order must be 'lsb-first' or 'msb-first'");
    }

    auto parse_packed_field(Declarations& declarations, FormId const form) -> PackedSegmentId {
        Fields const fields{syntax_, form, "field", 2};
        fields.validate({"bits", "kind", "range-helper", "minimum", "maximum", "default"},
                        {"code", "relation"});

        auto kind{PackedFieldKind::unsigned_integer};
        if (auto const value{fields.optional("kind")}) {
            auto const name{text(*value, "packed field kind")};
            if (name == "unsigned") {
                kind = PackedFieldKind::unsigned_integer;
            } else if (name == "signed") {
                kind = PackedFieldKind::signed_integer;
            } else if (name == "enum") {
                kind = PackedFieldKind::enumeration;
            } else if (name == "linear-quantized") {
                kind = PackedFieldKind::linear_quantized;
            } else if (name == "fixed-point") {
                kind = PackedFieldKind::fixed_point;
            } else if (name == "mini-float") {
                kind = PackedFieldKind::mini_float;
            } else {
                fail(*value,
                     "packed field kind must be 'unsigned', 'signed', 'enum', "
                     "'linear-quantized', 'fixed-point', or 'mini-float'");
            }
        }

        auto const minimum{fields.optional("minimum")};
        auto const maximum{fields.optional("maximum")};
        auto const& bits_form{fields.required("bits")};
        auto const bits_text{text(bits_form, "packed field bits")};
        auto const bits{bits_text == "auto"
                            ? std::optional<int>{}
                            : std::optional<int>{integer(bits_form, "packed field bits")}};
        std::vector<PackedNamedCodeId> named_codes;
        named_codes.reserve(fields.declarations().size());
        std::optional<SemanticRelationId> relationship;
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            if (syntax_.head(declaration) == "relation") {
                if (relationship.has_value()) {
                    fail(declaration, "packed field may have only one relationship");
                }
                Fields const relation{syntax_, declaration, "relation", 2};
                relation.validate({"unit"});
                auto const unit{relation.optional("unit")};
                {
                    auto& records{declarations.semantic_relations};
                    auto const record_id{records.append()};
                    auto const record_index{records.index(record_id)};
                    records.kind[record_index] =
                        parse_semantic_relation_kind(relation.positional(0));
                    records.target[record_index] =
                        parse_type_ref(declarations, relation.positional(1));
                    records.unit[record_index] =
                        !unit.has_value() ? std::nullopt
                                          : std::optional{parse_semantic_relation_unit(*unit)};
                    relationship = record_id;
                }
                continue;
            }
            Fields const code{syntax_, declaration, "code", 1};
            code.validate({"value", "sentinel"});
            {
                auto& records{declarations.packed_named_codes};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.name[record_index] =
                    identifier(code.positional(0), "packed named code name");
                records.value[record_index] =
                    packed_integer(code.required("value"), "packed named code value");
                records.sentinel[record_index] = boolean_or(code, "sentinel");
                named_codes.push_back(record_id);
            }
        }
        auto& records{declarations.packed_fields};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "packed field name");
        records.type[record_index] = parse_type_ref(declarations, fields.positional(1));
        records.bits[record_index] = bits;
        records.kind[record_index] = kind;
        records.range_helper[record_index] = boolean_or(fields, "range-helper");
        records.minimum_value[record_index] =
            !minimum.has_value() ? std::nullopt
                                 : std::optional<PackedIntegerValue>{
                                       packed_integer(*minimum, "packed field minimum")};
        records.maximum_value[record_index] =
            !maximum.has_value() ? std::nullopt
                                 : std::optional<PackedIntegerValue>{
                                       packed_integer(*maximum, "packed field maximum")};
        records.named_codes[record_index] = declarations.range(named_codes);
        records.relationship[record_index] = std::move(relationship);
        records.default_value[record_index] =
            !fields.optional("default").has_value()
                ? std::nullopt
                : std::optional{
                      packed_integer(*fields.optional("default"), "packed field default")};
        return record_id;
    }

    auto parse_packed_reserved_bits(Declarations& declarations, FormId const form)
        -> PackedSegmentId {
        Fields const fields{syntax_, form, "reserved", 1};
        fields.validate({"bits"});
        auto& records{declarations.packed_reserved_bits};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] =
            identifier(fields.positional(0), "packed reserved-region name");
        records.bits[record_index] =
            integer(fields.required("bits"), "packed reserved-region bits");
        return record_id;
    }

    auto parse_packed_value(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "packed-value", 1};
        fields.validate(
            {"storage", "invalid-value", "export-specifier", "mutable", "byte-order", "bit-order"},
            {"field", "reserved"});
        auto const invalid_value{fields.optional("invalid-value")};
        auto const byte_order{fields.optional("byte-order")};
        auto const bit_order{fields.optional("bit-order")};

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
        records.name[record_index] = identifier(fields.positional(0), "packed value name");
        records.storage_type[record_index] =
            parse_type_ref(declarations, fields.required("storage"));
        records.segments[record_index] = declarations.range(segments);
        records.invalid_value[record_index] = !invalid_value.has_value()
                                                ? std::nullopt
                                                : std::optional<std::uint64_t>{unsigned_integer(
                                                      *invalid_value, "packed invalid value")};
        records.export_specifier[record_index] = parse_export_specifier(fields);
        records.mutable_value[record_index] = boolean_or(fields, "mutable");
        records.byte_order[record_index] =
            !byte_order.has_value()
                ? std::nullopt
                : std::optional<PackedByteOrder>{parse_packed_byte_order(*byte_order)};
        records.bit_order[record_index] =
            !bit_order.has_value()
                ? std::nullopt
                : std::optional<PackedBitOrder>{parse_packed_bit_order(*bit_order)};
        return record_id;
    }

    auto parse_enum(Declarations& declarations, FormId const form) -> DeclarationHandle {
        auto const has_underlying_type{syntax_.children(form).size() > 2 &&
                                       syntax_.kind(syntax_.children(form)[2]) !=
                                           lispb::codegen::sexpr::TokenKind::keyword &&
                                       (!syntax_.is_list(syntax_.children(form)[2]) ||
                                        syntax_.head(syntax_.children(form)[2]) == "type-ref")};
        Fields const fields{syntax_, form, "enum", has_underlying_type ? 2U : 1U};
        fields.validate({"reflection",
                         "enum-array",
                         "count",
                         "conversions",
                         "export-specifier",
                         "native-api",
                         "helpers",
                         "bit-width",
                         "signed"},
                        {"value", "unreal-projection"});
        std::vector<EnumeratorId> values;
        std::optional<EnumUnrealProjectionId> unreal_projection;
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            if (syntax_.head(declaration) == "value") {
                Fields const value{syntax_, declaration, "value", 1};
                value.validate({"value", "display-name", "hidden", "serialized-name", "sentinel"});
                {
                    auto& records{declarations.enumerators};
                    auto const record_id{records.append()};
                    auto const record_index{records.index(record_id)};
                    records.name[record_index] = identifier(value.positional(0), "enumerator name");
                    records.initializer[record_index] = optional_text_id(value, "value");
                    records.display_name[record_index] = optional_text_id(value, "display-name");
                    records.hidden[record_index] = boolean_or(value, "hidden");
                    records.serialized_name[record_index] =
                        optional_text_id(value, "serialized-name");
                    records.sentinel[record_index] = boolean_or(value, "sentinel");
                    values.push_back(record_id);
                }
            } else {
                if (unreal_projection.has_value()) {
                    fail(declaration, "enum may define only one Unreal projection");
                }
                unreal_projection = parse_enum_unreal_projection(declarations, declaration);
            }
        }
        std::vector<NativeEnumHelper> helpers;
        if (auto const list{fields.optional("helpers")}) {
            if (!syntax_.is_list(*list)) {
                fail(*list, "helpers must be a list");
            }
            if (!boolean_or(fields, "native-api")) {
                fail(*list, "helpers requires :native-api true");
            }
            for (auto const& child : syntax_.children(*list)) {
                helpers.push_back(parse_native_enum_helper(child));
            }
        }
        std::vector<EnumConversion> conversions;
        if (auto const list{fields.optional("conversions")}) {
            if (!syntax_.is_list(*list)) {
                fail(*list, "conversions must be a list");
            }
            for (auto const& child : syntax_.children(*list)) {
                conversions.push_back(parse_enum_conversion(child));
            }
        }
        auto reflection{EnumReflection::none};
        if (auto const value{fields.optional("reflection")}) {
            reflection = parse_enum_reflection(*value);
        }
        std::optional<std::uint8_t> bit_width;
        if (auto const value{fields.optional("bit-width")}) {
            auto const parsed{unsigned_integer(*value, "enum bit width")};
            if (parsed == 0 || parsed > 64) {
                fail(*value, "enum bit width must be between 1 and 64 bits");
            }
            bit_width = static_cast<std::uint8_t>(parsed);
        }
        auto& records{declarations.enums};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "enum name");
        records.underlying_type[record_index] =
            has_underlying_type ? std::optional{parse_type_ref(declarations, fields.positional(1))}
                                : std::nullopt;
        records.values[record_index] = declarations.range(values);
        records.count[record_index] = optional_identifier(fields, "count");
        records.conversions[record_index] = declarations.range(conversions);
        records.export_specifier[record_index] = parse_export_specifier(fields);
        records.unreal_projection[record_index] = std::move(unreal_projection);
        records.bit_width[record_index] = bit_width;
        records.reflection[record_index] = reflection;
        records.signedness[record_index] = schema::EnumSignedness::unspecified;
        if (auto const signedness{fields.optional("signed")}) {
            records.signedness[record_index] = boolean(*signedness, "signed")
                                                 ? schema::EnumSignedness::signed_
                                                 : schema::EnumSignedness::unsigned_;
        }
        records.enum_array[record_index] = boolean_or(fields, "enum-array");
        records.native_api[record_index] = boolean_or(fields, "native-api");
        records.helpers[record_index] = declarations.range(helpers);
        return record_id;
    }

    auto parse_table(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "table", 1};
        fields.validate({"export-specifier"}, {"row", "column", "group"});
        std::vector<StaticTableRowId> rows;
        std::vector<StaticTableColumnId> columns;
        std::vector<StaticTableGroupId> groups;
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            auto const head{syntax_.head(declaration)};
            if (head == "row") {
                Fields const row{syntax_, declaration, "row", 1};
                row.validate({});
                {
                    auto& records{declarations.static_table_rows};
                    auto const record_id{records.append()};
                    auto const record_index{records.index(record_id)};
                    records.name[record_index] = identifier(row.positional(0), "row name");
                    rows.push_back(record_id);
                }
            } else if (head == "column") {
                Fields const column{syntax_, declaration, "column", 2};
                column.validate({});
                {
                    auto& records{declarations.static_table_columns};
                    auto const record_id{records.append()};
                    auto const record_index{records.index(record_id)};
                    records.name[record_index] = identifier(column.positional(0), "column name");
                    records.type[record_index] = parse_type_ref(declarations, column.positional(1));
                    columns.push_back(record_id);
                }
            } else if (head == "group") {
                Fields const group{syntax_, declaration, "group", 2};
                group.validate({"columns"});
                {
                    auto& records{declarations.static_table_groups};
                    auto const record_id{records.append()};
                    auto const record_index{records.index(record_id)};
                    records.name[record_index] = identifier(group.positional(0), "group name");
                    records.type[record_index] = parse_type_ref(declarations, group.positional(1));
                    records.columns[record_index] = declarations.range(
                        identifier_list(group.required("columns"), "group columns"));
                    groups.push_back(record_id);
                }
            }
        }
        auto& records{declarations.static_tables};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "table name");
        records.rows[record_index] = declarations.range(rows);
        records.columns[record_index] = declarations.range(columns);
        records.groups[record_index] = declarations.range(groups);
        records.export_specifier[record_index] = parse_export_specifier(fields);
        return record_id;
    }

    auto parse_facade_target_storage(FormId const form) -> bool {
        auto const value{text(form, "facade target storage")};
        if (value == "pointer") {
            return false;
        }
        if (value == "reference") {
            return true;
        }
        fail(form, "expected 'pointer' or 'reference'; got '" + value + "'");
    }

    auto parse_facade_access(Fields const& fields, std::string_view const property) -> Access {
        auto const form{fields.optional(property)};
        if (!form) {
            return Access::public_;
        }
        auto const value{text(*form, property)};
        if (value == "public") {
            return Access::public_;
        }
        if (value == "private") {
            return Access::private_;
        }
        fail(*form, "facade access must be public or private");
    }

    auto parse_facade_friend_kind(Fields const& fields) -> RecordKind {
        auto const form{fields.optional("friend-kind")};
        if (!form) {
            return RecordKind::class_;
        }
        auto const value{text(*form, "friend kind")};
        if (value == "class") {
            return RecordKind::class_;
        }
        if (value == "struct") {
            return RecordKind::struct_;
        }
        fail(*form, "facade friend_kind must be class or struct");
    }

    auto parse_facade(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "facade", 3};
        fields.validate({"validation",
                         "validation-dependencies",
                         "export-specifier",
                         "bind-access",
                         "method-access",
                         "friends",
                         "friend-kind",
                         "definitions-in-source",
                         "target-storage"},
                        {"method"});
        std::vector<FacadeMethodId> methods;
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            Fields const method{syntax_, declaration, "method", 2};
            method.validate({"const", "noexcept", "target-name"}, {"parameter"});
            std::vector<ParameterId> parameters;
            for (auto const parameter_id : method.declarations()) {
                auto const parameter{parameter_id};
                parameters.push_back(parse_parameter(declarations, parameter));
            }
            {
                auto& records{declarations.facade_methods};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.name[record_index] = identifier(method.positional(0), "facade method name");
                records.return_type[record_index] =
                    parse_type_ref(declarations, method.positional(1));
                records.parameters[record_index] = declarations.range(parameters);
                records.is_const[record_index] = boolean_or(method, "const");
                records.is_noexcept[record_index] = boolean_or(method, "noexcept");
                records.target_name[record_index] = optional_identifier(method, "target-name");
                methods.push_back(record_id);
            }
        }
        auto reference_target{false};
        if (auto const value{fields.optional("target-storage")}) {
            reference_target = parse_facade_target_storage(*value);
        }
        auto& records{declarations.facades};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "facade name");
        records.target_type[record_index] = parse_type_ref(declarations, fields.positional(1));
        records.target_member_name[record_index] =
            identifier(fields.positional(2), "facade target member name");
        records.methods[record_index] = declarations.range(methods);
        records.validation_lines[record_index] =
            declarations.range(cpp_lines_or(fields, "validation"));
        records.validation_dependencies[record_index] =
            declarations.range(identifier_list_or(fields, "validation-dependencies"));
        records.export_specifier[record_index] = parse_export_specifier(fields);
        records.bind_access[record_index] = parse_facade_access(fields, "bind-access");
        records.method_access[record_index] = parse_facade_access(fields, "method-access");
        records.friends[record_index] = declarations.range(text_id_list_or(fields, "friends"));
        records.friend_kind[record_index] = parse_facade_friend_kind(fields);
        records.definitions_in_source[record_index] = boolean_or(fields, "definitions-in-source");
        records.reference_target[record_index] = reference_target;
        return record_id;
    }

    auto parse_setting_apply_mode(FormId const form) -> SettingApplyMode {
        auto const value{text(form, "setting apply mode")};
        if (value == "immediate") {
            return SettingApplyMode::immediate;
        }
        if (value == "deferred") {
            return SettingApplyMode::deferred;
        }
        if (value == "confirm") {
            return SettingApplyMode::confirm;
        }
        fail(form, "unknown settings apply mode '" + value + "'");
    }

    auto parse_setting_device(FormId const form) -> SettingDevice {
        auto const value{text(form, "setting device")};
        if (value == "shared") {
            return SettingDevice::shared;
        }
        if (value == "keyboard-mouse") {
            return SettingDevice::keyboard_mouse;
        }
        if (value == "controller") {
            return SettingDevice::controller;
        }
        fail(form, "unknown setting device '" + value + "'");
    }

    auto parse_control(Declarations& declarations, FormId const form) -> SettingControlId {
        Fields const fields{syntax_, form, "control", 1};
        fields.validate(
            {"options-provider", "availability-provider", "min", "max", "step", "custom-row"});
        auto const kind_name{text(fields.positional(0), "setting control kind")};
        SettingControlKind kind;
        if (kind_name == "toggle") {
            kind = SettingControlKind::toggle;
        } else if (kind_name == "choice") {
            kind = SettingControlKind::choice;
        } else if (kind_name == "float-range") {
            kind = SettingControlKind::float_range;
        } else if (kind_name == "integer-range") {
            kind = SettingControlKind::integer_range;
        } else if (kind_name == "custom") {
            kind = SettingControlKind::custom;
        } else {
            fail(fields.positional(0), "unknown setting control kind '" + kind_name + "'");
        }
        auto optional_number = [&](std::string_view const name) -> std::optional<double> {
            auto const value{fields.optional(name)};
            return !value.has_value() ? std::nullopt : std::optional<double>{number(*value, name)};
        };
        auto& records{declarations.setting_controls};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.kind[record_index] = kind;
        records.options_provider[record_index] = optional_text_id(fields, "options-provider");
        records.availability_provider[record_index] =
            optional_text_id(fields, "availability-provider");
        records.minimum[record_index] = optional_number("min");
        records.maximum[record_index] = optional_number("max");
        records.step[record_index] = optional_number("step");
        records.custom_row[record_index] = optional_text_id(fields, "custom-row");
        return record_id;
    }

    auto parse_setting(Declarations& declarations, FormId const form) -> SettingId {
        Fields const fields{syntax_, form, "setting", 2};
        fields.validate({"tooltip", "category", "value-type", "backend", "apply", "device"},
                        {"control"});
        if (fields.declarations().size() != 1) {
            fail(form, "setting requires exactly one control declaration");
        }
        auto& records{declarations.settings};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "setting name");
        records.label[record_index] = text_id(fields.positional(1), "setting label");
        records.tooltip[record_index] = optional_text_id(fields, "tooltip");
        records.category[record_index] =
            identifier(fields.required("category"), "setting category");
        records.value_type[record_index] =
            parse_type_ref(declarations, fields.required("value-type"));
        records.backend[record_index] = identifier(fields.required("backend"), "setting backend");
        records.apply_mode[record_index] = parse_setting_apply_mode(fields.required("apply"));
        records.device[record_index] = fields.optional("device").has_value()
                                         ? parse_setting_device(*fields.optional("device"))
                                         : SettingDevice::shared;
        records.control[record_index] = parse_control(declarations, fields.declarations().front());
        return record_id;
    }

    auto parse_record_member(Declarations& declarations, FormId const form) -> RecordMemberId {
        Fields const fields{syntax_, form, "member", 2};
        fields.validate({"count", "initializer"}, {"relation"});
        auto const count{fields.optional("count")};
        std::optional<SemanticRelationId> relationship;
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            if (relationship.has_value()) {
                fail(declaration, "record member may have only one relationship");
            }
            Fields const relation{syntax_, declaration, "relation", 2};
            relation.validate({"unit"});
            auto const unit{relation.optional("unit")};
            {
                auto& records{declarations.semantic_relations};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.kind[record_index] = parse_semantic_relation_kind(relation.positional(0));
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
        records.name[record_index] = identifier(fields.positional(0), "record member name");
        records.type[record_index] = parse_type_ref(declarations, fields.positional(1));
        records.count[record_index] =
            !count.has_value()
                ? std::nullopt
                : std::optional<std::uint64_t>{unsigned_integer(*count, "record member count")};
        records.relationship[record_index] = std::move(relationship);
        records.initializer[record_index] = optional_text_id(fields, "initializer");
        return record_id;
    }

    auto parse_record(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "record", 1};
        fields.validate({"export-specifier", "comparison", "comparison-noexcept"},
                        {"member", "function"});
        auto comparison{RecordComparison::none};
        if (auto const value{optional_text(fields, "comparison")}) {
            if (*value == "equality") {
                comparison = RecordComparison::equality;
            } else if (*value == "three-way") {
                comparison = RecordComparison::three_way;
            } else {
                fail(fields.required("comparison"), "expected equality or three-way");
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
        records.name[record_index] = identifier(fields.positional(0), "record name");
        records.members[record_index] = declarations.range(members);
        records.export_specifier[record_index] = parse_export_specifier(fields);
        records.comparison[record_index] = comparison;
        records.comparison_noexcept[record_index] = boolean_or(fields, "comparison-noexcept");
        records.functions[record_index] = declarations.range(functions);
        return record_id;
    }

    auto parse_union_alternative(Declarations& declarations, FormId const form)
        -> UnionAlternativeId {
        Fields const fields{syntax_, form, "alternative", 2};
        fields.validate({"count"});
        auto const count{fields.optional("count")};
        auto& records{declarations.union_alternatives};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "union alternative name");
        records.type[record_index] = parse_type_ref(declarations, fields.positional(1));
        records.count[record_index] =
            !count.has_value()
                ? std::nullopt
                : std::optional<std::uint64_t>{unsigned_integer(*count, "union alternative count")};
        return record_id;
    }

    auto parse_union(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "union", 1};
        fields.validate({"export-specifier"}, {"alternative"});
        std::vector<UnionAlternativeId> alternatives;
        alternatives.reserve(fields.declarations().size());
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            alternatives.push_back(parse_union_alternative(declarations, declaration));
        }
        auto& records{declarations.unions};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "union name");
        records.alternatives[record_index] = declarations.range(alternatives);
        records.export_specifier[record_index] = parse_export_specifier(fields);
        return record_id;
    }

    auto parse_tagged_union_alternative(Declarations& declarations, FormId const form)
        -> TaggedUnionAlternativeId {
        Fields const fields{syntax_, form, "alternative", 2};
        fields.validate({"count", "tag"});
        auto const count{fields.optional("count")};
        auto& records{declarations.tagged_union_alternatives};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] =
            identifier(fields.positional(0), "tagged union alternative name");
        records.type[record_index] = parse_type_ref(declarations, fields.positional(1));
        records.count[record_index] =
            !count.has_value() ? std::nullopt
                               : std::optional<std::uint64_t>{
                                     unsigned_integer(*count, "tagged union alternative count")};
        records.tag[record_index] =
            identifier(fields.required("tag"), "tagged union alternative tag");
        return record_id;
    }

    auto parse_tagged_union(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "tagged-union", 1};
        fields.validate({"discriminant", "export-specifier"}, {"alternative"});
        std::vector<TaggedUnionAlternativeId> alternatives;
        alternatives.reserve(fields.declarations().size());
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            alternatives.push_back(parse_tagged_union_alternative(declarations, declaration));
        }
        auto& records{declarations.tagged_unions};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "tagged union name");
        records.discriminant[record_index] =
            parse_type_ref(declarations, fields.required("discriminant"));
        records.alternatives[record_index] = declarations.range(alternatives);
        records.export_specifier[record_index] = parse_export_specifier(fields);
        return record_id;
    }

    auto parse_integer_scalar(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "integer-scalar", 1};
        fields.validate({"signed", "minimum", "maximum", "bit-width", "cpp-emission", "cpp-type"},
                        {"code", "relation"});

        std::optional<std::uint32_t> bit_width;
        if (auto const width{fields.optional("bit-width")}) {
            auto const width_text{text(*width, "integer scalar bit width")};
            if (width_text != "auto") {
                auto const parsed{integer(*width, "integer scalar bit width")};
                if (parsed <= 0 || parsed > 64) {
                    fail(*width, "integer scalar bit width must be in the range 1..64");
                }
                bit_width = static_cast<std::uint32_t>(parsed);
            }
        }

        auto cpp_emission{IntegerScalarCppEmission::none};
        if (auto const emission{fields.optional("cpp-emission")}) {
            auto const emission_name{text(*emission, "integer scalar C++ emission policy")};
            if (emission_name == "constants") {
                cpp_emission = IntegerScalarCppEmission::constants;
            } else if (emission_name == "constants-with-names") {
                cpp_emission = IntegerScalarCppEmission::constants_with_names;
            } else if (emission_name == "alias") {
                cpp_emission = IntegerScalarCppEmission::alias;
            } else if (emission_name != "none") {
                fail(*emission,
                     "integer scalar C++ emission policy must be none, constants, "
                     "constants-with-names, or alias");
            }
        }
        auto const cpp_type{fields.optional("cpp-type")};

        std::vector<PackedNamedCodeId> named_codes;
        named_codes.reserve(fields.declarations().size());
        std::optional<SemanticRelationId> relationship;
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            if (syntax_.head(declaration) == "relation") {
                if (relationship.has_value()) {
                    fail(declaration, "integer scalar may have only one relationship");
                }
                Fields const relation{syntax_, declaration, "relation", 2};
                relation.validate({"unit"});
                auto const unit{relation.optional("unit")};
                {
                    auto& records{declarations.semantic_relations};
                    auto const record_id{records.append()};
                    auto const record_index{records.index(record_id)};
                    records.kind[record_index] =
                        parse_semantic_relation_kind(relation.positional(0));
                    records.target[record_index] =
                        parse_type_ref(declarations, relation.positional(1));
                    records.unit[record_index] =
                        !unit.has_value() ? std::nullopt
                                          : std::optional{parse_semantic_relation_unit(*unit)};
                    relationship = record_id;
                }
                continue;
            }
            Fields const code{syntax_, declaration, "code", 1};
            code.validate({"value", "sentinel"});
            {
                auto& records{declarations.packed_named_codes};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.name[record_index] =
                    identifier(code.positional(0), "integer scalar code name");
                records.value[record_index] =
                    packed_integer(code.required("value"), "integer scalar code value");
                records.sentinel[record_index] = boolean_or(code, "sentinel");
                named_codes.push_back(record_id);
            }
        }

        auto& records{declarations.integer_scalars};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "integer scalar name");
        records.signedness[record_index] =
            boolean(fields.required("signed"), "integer scalar signedness");
        records.minimum_value[record_index] =
            packed_integer(fields.required("minimum"), "integer scalar minimum");
        records.maximum_value[record_index] =
            packed_integer(fields.required("maximum"), "integer scalar maximum");
        records.bit_width[record_index] = bit_width;
        records.named_codes[record_index] = declarations.range(named_codes);
        records.relationship[record_index] = std::move(relationship);
        records.cpp_emission[record_index] = cpp_emission;
        records.cpp_type[record_index] = !cpp_type.has_value()
                                           ? std::nullopt
                                           : std::optional{parse_type_ref(declarations, *cpp_type)};
        return record_id;
    }

    auto parse_linear_quantized(Declarations& declarations, FormId const form)
        -> DeclarationHandle {
        Fields const fields{syntax_, form, "linear-quantized", 1};
        fields.validate({"source", "bits", "reserved-codes", "clipping"});

        auto clipping{QuantizationClipping::reject};
        if (auto const value{fields.optional("clipping")}) {
            auto const name{text(*value, "linear quantization clipping policy")};
            if (name == "clamp") {
                clipping = QuantizationClipping::clamp;
            } else if (name != "reject") {
                fail(*value, "linear quantization clipping must be reject or clamp");
            }
        }

        auto const bits{integer(fields.required("bits"), "linear quantization bit width")};
        if (bits <= 0 || bits > 64) {
            fail(fields.required("bits"),
                 "linear quantization bit width must be in the range 1..64");
        }
        auto const reserved{fields.optional("reserved-codes")};
        auto& records{declarations.linear_quantizeds};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "linear quantization name");
        records.source[record_index] = parse_type_ref(declarations, fields.required("source"));
        records.bit_width[record_index] = static_cast<std::uint32_t>(bits);
        records.reserved_codes[record_index] =
            reserved.has_value()
                ? unsigned_integer(*reserved, "linear quantization reserved code count")
                : 0;
        records.clipping[record_index] = clipping;
        return record_id;
    }

    auto parse_integer_varint(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "integer-varint", 1};
        fields.validate({"source", "encoding"});

        auto const encoding_name{text(fields.required("encoding"), "integer varint encoding")};
        IntegerVarintEncoding encoding;
        if (encoding_name == "unsigned") {
            encoding = IntegerVarintEncoding::unsigned_varint;
        } else if (encoding_name == "signed") {
            encoding = IntegerVarintEncoding::signed_varint;
        } else if (encoding_name == "zigzag") {
            encoding = IntegerVarintEncoding::zigzag_varint;
        } else {
            fail(fields.required("encoding"),
                 "integer varint encoding must be unsigned, signed, or zigzag");
        }

        auto& records{declarations.integer_varints};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "integer varint name");
        records.source[record_index] = parse_type_ref(declarations, fields.required("source"));
        records.encoding[record_index] = encoding;
        return record_id;
    }

    auto parse_fixed_point(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "fixed-point", 1};
        fields.validate(
            {"signed", "total-bits", "fractional-bits", "rounding", "minimum", "maximum"});

        auto const total_bits{integer(fields.required("total-bits"), "fixed-point total width")};
        if (total_bits <= 0 || total_bits > 64) {
            fail(fields.required("total-bits"),
                 "fixed-point total width must be in the range 1..64");
        }
        auto const fractional_bits{
            integer(fields.required("fractional-bits"), "fixed-point fractional width")};
        if (fractional_bits < 0 || fractional_bits > 64) {
            fail(fields.required("fractional-bits"),
                 "fixed-point fractional width must be in the range 0..64");
        }

        auto rounding{FixedPointRounding::nearest_even};
        if (auto const value{fields.optional("rounding")}) {
            auto const name{text(*value, "fixed-point rounding policy")};
            if (name == "toward-zero") {
                rounding = FixedPointRounding::toward_zero;
            } else if (name != "nearest-even") {
                fail(*value, "fixed-point rounding must be nearest-even or toward-zero");
            }
        }

        auto const minimum{fields.optional("minimum")};
        auto const maximum{fields.optional("maximum")};
        auto& records{declarations.fixed_points};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "fixed-point name");
        records.signedness[record_index] =
            boolean(fields.required("signed"), "fixed-point signedness");
        records.total_bits[record_index] = static_cast<std::uint32_t>(total_bits);
        records.fractional_bits[record_index] = static_cast<std::uint32_t>(fractional_bits);
        records.rounding[record_index] = rounding;
        records.minimum_value[record_index] =
            !minimum.has_value() ? std::nullopt
                                 : std::optional{text_id(*minimum, "fixed-point minimum")};
        records.maximum_value[record_index] =
            !maximum.has_value() ? std::nullopt
                                 : std::optional{text_id(*maximum, "fixed-point maximum")};
        return record_id;
    }

    auto parse_mini_float(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "mini-float", 1};
        fields.validate({"sign-bits", "exponent-bits", "significand-bits", "bias"});

        auto const sign_bits{integer(fields.required("sign-bits"), "mini-float sign width")};
        if (sign_bits < 0 || sign_bits > 1) {
            fail(fields.required("sign-bits"), "mini-float sign width must be zero or one");
        }
        auto const exponent_bits{
            integer(fields.required("exponent-bits"), "mini-float exponent width")};
        if (exponent_bits < 2 || exponent_bits > 15) {
            fail(fields.required("exponent-bits"),
                 "mini-float exponent width must be in the range 2..15");
        }
        auto const significand_bits{
            integer(fields.required("significand-bits"), "mini-float significand width")};
        if (significand_bits < 0 || significand_bits > 62) {
            fail(fields.required("significand-bits"),
                 "mini-float significand width must be in the range 0..62");
        }
        if (sign_bits + exponent_bits + significand_bits > 64) {
            fail(form, "mini-float total width must not exceed 64 bits");
        }
        auto const exponent_bias{integer(fields.required("bias"), "mini-float exponent bias")};
        if (exponent_bias < -32'768 || exponent_bias > 32'767) {
            fail(fields.required("bias"),
                 "mini-float exponent bias must be in the range -32768..32767");
        }

        auto& records{declarations.mini_floats};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "mini-float name");
        records.sign_bits[record_index] = static_cast<std::uint32_t>(sign_bits);
        records.exponent_bits[record_index] = static_cast<std::uint32_t>(exponent_bits);
        records.significand_bits[record_index] = static_cast<std::uint32_t>(significand_bits);
        records.exponent_bias[record_index] = exponent_bias;
        return record_id;
    }

    auto parse_optional_sentinel(Declarations& declarations, FormId const form)
        -> DeclarationHandle {
        Fields const fields{syntax_, form, "optional-sentinel", 1};
        fields.validate({"source", "sentinel"});

        auto& records{declarations.optional_sentinels};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "optional sentinel name");
        records.source[record_index] = parse_type_ref(declarations, fields.required("source"));
        records.sentinel[record_index] =
            identifier(fields.required("sentinel"), "optional sentinel code");
        return record_id;
    }

    auto parse_optional_presence_bit(Declarations& declarations, FormId const form)
        -> DeclarationHandle {
        Fields const fields{syntax_, form, "optional-presence-bit", 1};
        fields.validate({"source"});

        auto& records{declarations.optional_presence_bits};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "optional presence-bit name");
        records.source[record_index] = parse_type_ref(declarations, fields.required("source"));
        return record_id;
    }

    auto parse_layout(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const layout{syntax_, form, "layout", 1};
        layout.validate({"components", "input-members", "export-specifier"}, {"value-type"});
        std::vector<HomogeneousValueId> value_types;
        for (auto const value_declaration_id : layout.declarations()) {
            auto const value_declaration{value_declaration_id};
            Fields const value{syntax_, value_declaration, "value-type", 2};
            value.validate({"equivalent-type", "input-types"});
            std::optional<TypeRefId> equivalent;
            if (auto const equivalent_value{value.optional("equivalent-type")}) {
                equivalent = parse_type_ref(declarations, *equivalent_value);
            }
            std::vector<TypeRefId> inputs;
            if (auto const input_values{value.optional("input-types")}) {
                inputs = type_ref_list(declarations, *input_values, "input types");
            }
            {
                auto& records{declarations.homogeneous_values};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.type[record_index] = parse_type_ref(declarations, value.positional(0));
                records.suffix[record_index] =
                    text_id(value.positional(1), "homogeneous type suffix");
                records.equivalent_type[record_index] = std::move(equivalent);
                records.input_types[record_index] = declarations.range(inputs);
                value_types.push_back(record_id);
            }
        }
        auto& records{declarations.homogeneous_layouts};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(layout.positional(0), "homogeneous layout name");
        records.components[record_index] =
            declarations.range(identifier_list(layout.required("components"), "layout components"));
        records.input_members[record_index] =
            declarations.range(identifier_list_or(layout, "input-members"));
        records.value_types[record_index] = declarations.range(value_types);
        records.export_specifier[record_index] = parse_export_specifier(layout);
        return record_id;
    }

    auto parse_vector_soa(Declarations& declarations, FormId const form) -> DeclarationHandle {
        Fields const fields{syntax_, form, "vector-soa", 1};
        fields.validate({"value-type",
                         "components",
                         "equivalent-members",
                         "equivalent-accessors",
                         "equivalent-constructor",
                         "equivalent-type",
                         "export-specifier"},
                        {"fixed"});
        std::optional<FixedSoaId> fixed;
        if (!fields.declarations().empty()) {
            if (fields.declarations().size() != 1) {
                fail(form, "vector SOA accepts at most one fixed declaration");
            }
            fixed = parse_fixed(declarations, fields.declarations().front());
        }
        auto& records{declarations.vector_soas};
        auto const record_id{records.append()};
        auto const record_index{records.index(record_id)};
        records.name[record_index] = identifier(fields.positional(0), "vector storage name");
        records.value_type[record_index] =
            parse_type_ref(declarations, fields.required("value-type"));
        records.components[record_index] =
            declarations.range(identifier_list(fields.required("components"), "vector components"));
        records.equivalent_members[record_index] =
            declarations.range(identifier_list_or(fields, "equivalent-members"));
        records.equivalent_constructor[record_index] =
            optional_text_id(fields, "equivalent-constructor");
        records.equivalent_type[record_index] =
            parse_type_ref(declarations, fields.required("equivalent-type"));
        records.export_specifier[record_index] = parse_export_specifier(fields);
        records.fixed[record_index] = std::move(fixed);
        records.equivalent_accessors[record_index] = boolean_or(fields, "equivalent-accessors");
        return record_id;
    }

    auto parse_normal_declaration(Declarations& declarations, FormId const form)
        -> DeclarationHandle {
        auto const head{syntax_.head(form)};
        if (head == "enum") {
            return parse_enum(declarations, form);
        }
        if (head == "integer-scalar") {
            return parse_integer_scalar(declarations, form);
        }
        if (head == "linear-quantized") {
            return parse_linear_quantized(declarations, form);
        }
        if (head == "integer-varint") {
            return parse_integer_varint(declarations, form);
        }
        if (head == "fixed-point") {
            return parse_fixed_point(declarations, form);
        }
        if (head == "mini-float") {
            return parse_mini_float(declarations, form);
        }
        if (head == "optional-sentinel") {
            return parse_optional_sentinel(declarations, form);
        }
        if (head == "optional-presence-bit") {
            return parse_optional_presence_bit(declarations, form);
        }
        if (head == "packed-value") {
            return parse_packed_value(declarations, form);
        }
        if (head == "record") {
            return parse_record(declarations, form);
        }
        if (head == "union") {
            return parse_union(declarations, form);
        }
        if (head == "tagged-union") {
            return parse_tagged_union(declarations, form);
        }
        if (head == "struct") {
            return parse_soa(declarations, form);
        }
        if (head == "vector-soa") {
            return parse_vector_soa(declarations, form);
        }
        if (head == "layout") {
            return parse_layout(declarations, form);
        }
        if (head == "table") {
            return parse_table(declarations, form);
        }
        if (head == "facade") {
            return parse_facade(declarations, form);
        }
        fail(form, "unknown normal module declaration '" + std::string{head} + "'");
    }

    auto parse_module(Declarations& declarations, FormId const form) -> ModuleHandle {
        auto const head{syntax_.head(form)};
        if (head == "module") {
            Fields const fields{syntax_, form, head, 1};
            fields.validate({"header",
                             "source",
                             "header-include",
                             "namespace",
                             "include-order",
                             "prelude",
                             "helper-namespace",
                             "backend"},
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
                             "vector-soa",
                             "layout",
                             "table",
                             "facade",
                             "array-allocator"});
            auto& modules{declarations.normal_modules};
            auto const module{modules.append()};
            auto const module_index{modules.index(module)};
            modules.settings[module_index] = parse_module_settings(declarations, fields);
            modules.enum_helper_namespace[module_index] =
                optional_identifier(fields, "helper-namespace");
            if (auto const backend{optional_text(fields, "backend")}) {
                if (*backend == "standard-library") {
                    modules.soa_backend[module_index] = SoaBackend::standard_library;
                } else if (*backend != "unreal") {
                    fail(fields.required("backend"),
                         "SOA backend must be unreal or standard-library");
                }
            }
            std::vector<SoaAllocatorVariantId> allocators;
            std::vector<DeclarationHandle> module_declarations;
            for (auto const declaration_id : fields.declarations()) {
                auto const declaration{declaration_id};
                if (syntax_.head(declaration) == "array-allocator") {
                    Fields const allocator{syntax_, declaration, "array-allocator", 2};
                    allocator.validate({});
                    {
                        auto& records{declarations.soa_allocator_variants};
                        auto const record_id{records.append()};
                        auto const record_index{records.index(record_id)};
                        records.prefix[record_index] =
                            identifier(allocator.positional(0), "allocator prefix");
                        records.allocator[record_index] =
                            parse_type_ref(declarations, allocator.positional(1));
                        allocators.push_back(record_id);
                    }
                } else {
                    module_declarations.push_back(
                        parse_normal_declaration(declarations, declaration));
                }
            }
            modules.soa_array_allocators[module_index] = declarations.range(allocators);
            modules.declarations[module_index] = declarations.range(module_declarations);
            return module;
        }
        if (head == "settings-module") {
            Fields const fields{syntax_, form, head, 1};
            fields.validate({"header",
                             "source",
                             "header-include",
                             "namespace",
                             "include-order",
                             "prelude",
                             "api-name",
                             "state-name",
                             "export-specifier"},
                            {"category", "setting"});
            std::vector<SettingCategoryId> categories;
            std::vector<SettingId> settings;
            for (auto const declaration_id : fields.declarations()) {
                auto const declaration{declaration_id};
                if (syntax_.head(declaration) == "category") {
                    Fields const category{syntax_, declaration, "category", 2};
                    category.validate({});
                    {
                        auto& records{declarations.setting_categories};
                        auto const record_id{records.append()};
                        auto const record_index{records.index(record_id)};
                        records.name[record_index] =
                            identifier(category.positional(0), "category name");
                        records.label[record_index] =
                            text_id(category.positional(1), "category label");
                        categories.push_back(record_id);
                    }
                } else {
                    settings.push_back(parse_setting(declarations, declaration));
                }
            }
            auto& records{declarations.settings_modules};
            auto const record_id{records.append()};
            auto const record_index{records.index(record_id)};
            records.settings[record_index] = parse_module_settings(declarations, fields);
            records.api_name[record_index] =
                identifier(fields.required("api-name"), "settings API name");
            records.state_name[record_index] =
                identifier(fields.required("state-name"), "settings state name");
            records.export_specifier[record_index] = parse_export_specifier(fields);
            records.categories[record_index] = declarations.range(categories);
            records.settings_list[record_index] = declarations.range(settings);
            return record_id;
        }
        if (head == "umbrella-module") {
            Fields const fields{syntax_, form, head, 1};
            fields.validate({"header",
                             "source",
                             "header-include",
                             "namespace",
                             "include-order",
                             "prelude",
                             "headers"});
            auto& records{declarations.umbrella_modules};
            auto const record_id{records.append()};
            auto const record_index{records.index(record_id)};
            records.settings[record_index] = parse_module_settings(declarations, fields);
            records.headers[record_index] =
                declarations.range(text_id_list(fields.required("headers"), "umbrella headers"));
            return record_id;
        }
        fail(form, "unknown module declaration '" + std::string{head} + "'");
    }

    auto parse_external_integer(RegisteredTypes& registered, FormId const form)
        -> ExternalScalarId {
        auto& declarations{registered.declarations};
        Fields const fields{syntax_, form, "integer", 0};
        fields.validate({"signed", "bit-width", "minimum", "maximum"}, {"code"});
        auto const width{integer(fields.required("bit-width"), "external integer bit width")};
        if (width < 1 || width > 64) {
            fail(form, "external integer bit width must be in the range 1..64");
        }
        auto const signedness{boolean(fields.required("signed"), "external integer signedness")};
        auto const magnitude{std::uint64_t{1} << (width - 1)};
        auto const maximum{signedness    ? magnitude - 1
                           : width == 64 ? (std::numeric_limits<std::uint64_t>::max)()
                                         : (std::uint64_t{1} << width) - 1};
        auto minimum_value{signedness ? PackedIntegerValue::from_parts(true, magnitude)
                                      : PackedIntegerValue{0}};
        auto maximum_value{PackedIntegerValue{maximum}};
        if (auto const minimum{fields.optional("minimum")}) {
            minimum_value = packed_integer(*minimum, "external integer minimum");
        }
        if (auto const maximum_form{fields.optional("maximum")}) {
            maximum_value = packed_integer(*maximum_form, "external integer maximum");
        }
        std::vector<PackedNamedCodeId> named_codes;
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            Fields const code{syntax_, declaration, "code", 1};
            code.validate({"value", "sentinel"});
            {
                auto& records{declarations.packed_named_codes};
                auto const record_id{records.append()};
                auto const record_index{records.index(record_id)};
                records.name[record_index] = identifier(code.positional(0), "code name");
                records.value[record_index] = packed_integer(code.required("value"), "code value");
                records.sentinel[record_index] = boolean_or(code, "sentinel");
                named_codes.push_back(record_id);
            }
        }
        return registered.add_integer(signedness,
                                      minimum_value,
                                      maximum_value,
                                      static_cast<std::uint8_t>(width),
                                      declarations.range(named_codes));
    }

    auto parse_type_definition(FormId const form, TypeRegistry& registry)
        -> std::pair<lispb::IdentifierId, RegisteredTypeId> {
        Fields const fields{syntax_, form, "type", 1};
        fields.validate({"spelling", "header", "pass-by"},
                        {"operation", "integer", "floating-point"});
        ExternalScalarId semantics;
        auto& cpp_types{registry.cpp_types};
        auto const spelling{text(fields.required("spelling"), "type spelling")};
        std::vector<TypeDependencyId> dependencies;
        if (auto const header{optional_text(fields, "header")}) {
            dependencies.push_back(cpp_types.dependency(spelling, *header));
        }
        auto const type{cpp_types.add(spelling, dependencies)};
        if (auto const pass_by{fields.optional("pass-by")}) {
            cpp_types.set_parameter_passing(type, parse_parameter_passing(*pass_by));
        }
        for (auto const declaration_id : fields.declarations()) {
            auto const declaration{declaration_id};
            if (syntax_.head(declaration) != "operation") {
                if (semantics.kind() != ExternalScalarKind::none) {
                    fail(declaration, "type may have only one scalar description");
                }
                if (syntax_.head(declaration) == "integer") {
                    semantics = parse_external_integer(registry.registered, declaration);
                } else {
                    Fields const floating{syntax_, declaration, "floating-point", 0};
                    floating.validate({"format"});
                    auto const format{text(floating.required("format"), "floating-point format")};
                    if (format == "ieee754-binary16") {
                        semantics =
                            registry.registered.add_floating(FloatingPointFormat::ieee754_binary16);
                    } else if (format == "ieee754-binary32") {
                        semantics =
                            registry.registered.add_floating(FloatingPointFormat::ieee754_binary32);
                    } else if (format == "ieee754-binary64") {
                        semantics =
                            registry.registered.add_floating(FloatingPointFormat::ieee754_binary64);
                    } else {
                        fail(declaration, "unsupported floating-point format '" + format + "'");
                    }
                }
                continue;
            }
            Fields const operation{syntax_, declaration, "operation", 2};
            operation.validate({"pass-by"});
            auto const operation_name{text(operation.positional(0), "type operation")};
            TypeOperation kind;
            if (operation_name == "add-element") {
                kind = TypeOperation::add_element;
            } else if (operation_name == "remove-at-swap") {
                kind = TypeOperation::remove_at_swap;
            } else if (operation_name == "set-element") {
                kind = TypeOperation::set_element;
            } else {
                fail(operation.positional(0), "unknown type operation '" + operation_name + "'");
            }
            if (cpp_types.operation(type, kind).has_value()) {
                fail(declaration, "duplicate type operation '" + operation_name + "'");
            }
            auto passing{ParameterPassing::const_reference};
            if (kind != TypeOperation::remove_at_swap) {
                auto const pass_by{operation.optional("pass-by")};
                if (pass_by.has_value()) {
                    passing = parse_parameter_passing(*pass_by);
                }
            } else if (operation.optional("pass-by").has_value()) {
                fail(*operation.optional("pass-by"), "remove-at-swap does not accept ':pass-by'");
            }
            cpp_types.set_operation(
                type, kind, text(operation.positional(1), "type operation function"), passing);
        }
        return {identifier(fields.positional(0), "type name"),
                registry.registered.append(type, semantics)};
    }
  private:
    Syntax& syntax_;
};

} // namespace

auto load_type_registry(lispb::CompilerStorage& storage, std::filesystem::path const& root)
    -> LoadedTypeRegistry {
    auto& syntax{storage.syntax};
    SourceParser parser{syntax};
    LoadedTypeRegistry result{
        .types = TypeRegistry{storage.registered_types}, .sources = {}, .declarations = {}};
    std::map<std::filesystem::path, sexpr::SourceFileId> loaded;
    std::set<std::filesystem::path> active;
    auto load = [&](auto&& self, std::filesystem::path const& source_path) -> sexpr::SourceFileId {
        auto const path{std::filesystem::weakly_canonical(source_path)};
        if (active.contains(path)) {
            throw ManifestError{"Cyclic type registry include: " + path.string()};
        }
        if (auto const found{loaded.find(path)}; found != loaded.end()) {
            return found->second;
        }
        active.insert(path);
        auto const file{syntax.sources().load(path)};
        syntax.sources().set_kind(file, sexpr::SourceFileKind::type_registry);
        loaded.emplace(path, file);
        auto const forms{sexpr::read_forms(syntax, file)};
        result.sources.push_back(file);
        std::vector<sexpr::SourceFileId> include_targets;
        std::vector<sexpr::SourceSpan> include_spans;
        for (std::uint32_t form_index{}; form_index < forms.count; ++form_index) {
            auto const form{syntax.forms(forms)[form_index]};
            if (syntax.head(form) == "include") {
                Fields const include{syntax, form, "include", 1};
                include.validate({});
                auto const target{std::filesystem::weakly_canonical(
                    path.parent_path() / parser.text(include.positional(0), "include path"))};
                try {
                    auto const target_file{self(self, target)};
                    include_targets.push_back(target_file);
                    include_spans.push_back(syntax.span(form));
                } catch (std::exception const& error) {
                    parser.fail(form, error.what());
                }
                continue;
            }
            auto [name, type]{parser.parse_type_definition(form, result.types)};
            if (!result.types.bind(name, type)) {
                parser.fail(form,
                            "duplicate type definition '" +
                                std::string{storage.strings.view(name)} + "'");
            }
            result.declarations.emplace(name, form);
        }
        syntax.sources().set_includes(file, include_targets, include_spans);
        active.erase(path);
        return file;
    };
    try {
        load(load, root);
    } catch (sexpr::SourceError const& error) {
        throw ManifestError{error.what()};
    }
    return result;
}

auto parse_module_source(lispb::CompilerStorage& storage, sexpr::SourceFileId const file)
    -> LoadedModuleSource {
    auto& syntax{storage.syntax};
    syntax.sources().set_kind(file, sexpr::SourceFileKind::module);
    SourceParser parser{syntax};
    LoadedModuleSource result{.file = file, .forms = {}, .modules = {}};
    try {
        auto const forms{sexpr::read_forms(storage.syntax, file)};
        result.forms = forms;
        for (auto const form : syntax.forms(forms)) {
            result.modules.push_back(parser.parse_module(storage.declarations, form));
        }
    } catch (sexpr::SourceError const& error) {
        throw ManifestError{error.what()};
    }
    return result;
}

auto load_module_source(lispb::CompilerStorage& storage, std::filesystem::path const& path)
    -> LoadedModuleSource {
    auto const file{storage.sources.load(path)};
    if (detail::module_source_read_hook != nullptr) {
        detail::module_source_read_hook(path);
    }
    return parse_module_source(storage, file);
}

auto load_sources(lispb::CompilerStorage& storage,
                  std::filesystem::path const& types_path,
                  std::span<std::filesystem::path const> const module_paths) -> Manifest {
    return load_sources(storage, load_type_registry(storage, types_path), module_paths);
}

auto load_sources(lispb::CompilerStorage& storage,
                  LoadedTypeRegistry const& registry,
                  std::span<std::filesystem::path const> const module_paths) -> Manifest {
    std::vector<ModuleHandle> modules;
    for (auto const& module_path : module_paths) {
        auto source{load_module_source(storage, module_path)};
        modules.insert(modules.end(), source.modules.begin(), source.modules.end());
    }
    return Manifest{
        manifest_schema_version, registry.types, storage.declarations, std::move(modules)};
}

} // namespace lispb::codegen
