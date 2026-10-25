#include "validation.h"

#include "packed_value_internal.h"
#include "soa_body.h"

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/type_operation.h>
#include <codegen/path_utils.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enum_conversion.h>
#include <codegen/schema/enum_reflection.h>
#include <codegen/schema/enum_schemas.h>
#include <codegen/schema/enum_unreal_projections.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/external_scalar_kind.h>
#include <codegen/schema/facade_method_schemas.h>
#include <codegen/schema/facade_schemas.h>
#include <codegen/schema/fixed_point_schemas.h>
#include <codegen/schema/fixed_point_value.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/integer_scalar_cpp_emission.h>
#include <codegen/schema/integer_scalar_schemas.h>
#include <codegen/schema/integer_varint_encoding.h>
#include <codegen/schema/integer_varint_schemas.h>
#include <codegen/schema/linear_quantized_schemas.h>
#include <codegen/schema/manifest.h>
#include <codegen/schema/mini_float_schemas.h>
#include <codegen/schema/module_handle.h>
#include <codegen/schema/module_settings_store.h>
#include <codegen/schema/native_enum_helper.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/optional_presence_bit_schemas.h>
#include <codegen/schema/optional_sentinel_schemas.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_field_schemas.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_reserved_bits_schemas.h>
#include <codegen/schema/packed_segment_kind.h>
#include <codegen/schema/packed_value_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/record_comparison.h>
#include <codegen/schema/record_member_schemas.h>
#include <codegen/schema/record_schemas.h>
#include <codegen/schema/registered_types.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/schema_version.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <codegen/schema/semantic_relation_schemas.h>
#include <codegen/schema/setting_category_schemas.h>
#include <codegen/schema/setting_control_kind.h>
#include <codegen/schema/setting_control_schemas.h>
#include <codegen/schema/setting_schemas.h>
#include <codegen/schema/settings_module_schemas.h>
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <codegen/schema/static_table_column_schemas.h>
#include <codegen/schema/static_table_group_schemas.h>
#include <codegen/schema/static_table_row_schemas.h>
#include <codegen/schema/static_table_schemas.h>
#include <codegen/schema/tagged_union_alternative_schemas.h>
#include <codegen/schema/tagged_union_schemas.h>
#include <codegen/schema/type_refs.h>
#include <codegen/schema/type_registry.h>
#include <codegen/schema/umbrella_module_schemas.h>
#include <codegen/schema/union_alternative_schemas.h>
#include <codegen/schema/union_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/enum_code.h>
#include <lispb/schema/enum_domain_issue_severity.h>
#include <lispb/schema/enum_domains.h>
#include <lispb/schema/enum_storage_requirement.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

class Validator {
  public:
    Validator(Manifest const& manifest, lispb::schema::TypeGraph const& graph)
        : manifest_{manifest}
        , graph_{graph}
        , declarations_{manifest.declarations}
        , strings_{declarations_.strings()} {}

    struct EnumStorageDomain {
        std::uint32_t bit_width{};
        bool is_unsigned{};
    };

    auto text(lispb::StringId id) const -> std::string { return std::string{strings_.view(id)}; }

    auto view(lispb::StringId id) const -> std::string_view { return strings_.view(id); }
    auto resolved_type(TypeRefId id, TypeRegistry const&) const -> CppTypeId {
        auto const occurrence{graph_.resolved(module_, id)};
        auto const& occurrences{graph_.occurrences()};
        return occurrences.cpp_type[occurrences.index(occurrence)];
    }
    void require_value(lispb::StringId id, std::string const& context) {
        require_value(view(id), context);
    }
    void require_non_blank_value(lispb::StringId id, std::string const& context) {
        require_non_blank_value(view(id), context);
    }
    void require_identifier(lispb::StringId id, std::string const& context) {
        require_identifier(view(id), context);
    }
    void require_qualified_identifier(lispb::StringId id, std::string const& context) {
        require_qualified_identifier(view(id), context);
    }
    template <typename Id>
    void require_unique_names(SchemaRange<Id> names, std::string const& context) {
        std::set<lispb::StringId> unique;
        for (auto const name : declarations_.items(names)) {
            require_value(name, context);
            if (!unique.insert(name).second) {
                throw std::invalid_argument{context + " contains duplicate name: " + text(name)};
            }
        }
    }
    void require_value(std::string_view value, std::string const& context) {
        if (value.empty()) {
            throw std::invalid_argument{context + " must not be empty"};
        }
    }

    void require_non_blank_value(std::string_view value, std::string const& context) {
        require_value(value, context);
        if (value.find_first_not_of(" \t\r\n") == std::string_view::npos) {
            throw std::invalid_argument{context + " must not be blank"};
        }
    }

    auto is_identifier_start(unsigned char const character) -> bool {
        return std::isalpha(character) != 0 || character == '_';
    }

    static auto is_identifier_character(unsigned char const character) -> bool {
        return std::isalnum(character) != 0 || character == '_';
    }

    auto is_cpp_keyword(std::string_view const value) -> bool {
        static std::set<std::string_view> const keywords{
            "alignas",
            "alignof",
            "and",
            "and_eq",
            "asm",
            "auto",
            "bitand",
            "bitor",
            "bool",
            "break",
            "case",
            "catch",
            "char",
            "char8_t",
            "char16_t",
            "char32_t",
            "class",
            "compl",
            "concept",
            "const",
            "consteval",
            "constexpr",
            "constinit",
            "const_cast",
            "continue",
            "co_await",
            "co_return",
            "co_yield",
            "decltype",
            "default",
            "delete",
            "do",
            "double",
            "dynamic_cast",
            "else",
            "enum",
            "explicit",
            "export",
            "extern",
            "false",
            "float",
            "for",
            "friend",
            "goto",
            "if",
            "import",
            "inline",
            "int",
            "long",
            "module",
            "mutable",
            "namespace",
            "new",
            "noexcept",
            "not",
            "not_eq",
            "nullptr",
            "operator",
            "or",
            "or_eq",
            "private",
            "protected",
            "public",
            "register",
            "reinterpret_cast",
            "requires",
            "return",
            "short",
            "signed",
            "sizeof",
            "static",
            "static_assert",
            "static_cast",
            "struct",
            "switch",
            "template",
            "this",
            "thread_local",
            "throw",
            "true",
            "try",
            "typedef",
            "typeid",
            "typename",
            "union",
            "unsigned",
            "using",
            "virtual",
            "void",
            "volatile",
            "wchar_t",
            "while",
            "xor",
            "xor_eq",
        };
        return keywords.contains(value);
    }

    auto is_reserved_identifier(std::string_view const value) -> bool {
        return value.starts_with("__") || (value.size() > 1 && value.front() == '_' &&
                                           std::isupper(static_cast<unsigned char>(value[1])) != 0);
    }

    auto is_generated_storage_member_name(std::string_view const value) -> bool {
        static std::set<std::string_view> const names{
            "apply_permutation",
            "copy_element",
            "copy_elements",
            "get_const_view",
            "get_view",
            "is_empty",
            "left",
            "num",
            "right",
            "slice",
            "sort",
            "stable_sort",
            "resolve",
            "validate",
            "capacity",
            "allocated_bytes",
            "get_memory_resource",
            "each_column",
            "soa_schema",
            "resolved_view_type",
            "column_count",
        };
        return names.contains(value);
    }

    auto is_generated_mutation_name(std::string_view const value) -> bool {
        static std::set<std::string_view> const names{
            "add",
            "add_defaulted",
            "add_uninitialised",
            "append_from",
            "remove_at_swap",
            "reserve",
            "reset",
            "set",
            "set_num",
            "set_num_uninitialised",
        };
        return names.contains(value);
    }

    void reject_generated_name_collision(std::string_view const value,
                                         std::string const& context,
                                         bool const include_mutations) {
        if (is_generated_storage_member_name(value) ||
            (include_mutations && is_generated_mutation_name(value))) {
            throw std::invalid_argument{context +
                                        " collides with generated API: " + std::string{value}};
        }
    }

    void require_identifier(std::string_view value, std::string const& context) {
        require_value(value, context);
        if (!is_identifier_start(static_cast<unsigned char>(value.front())) ||
            !std::ranges::all_of(value.substr(1), [](char const character) {
                return is_identifier_character(static_cast<unsigned char>(character));
            })) {
            throw std::invalid_argument{context +
                                        " must be a C++ identifier: " + std::string{value}};
        }
        if (is_cpp_keyword(value) || is_reserved_identifier(value)) {
            throw std::invalid_argument{
                context + " must not be a reserved C++ identifier: " + std::string{value}};
        }
    }

    void require_qualified_identifier(std::string_view value, std::string const& context) {
        require_value(value, context);
        std::size_t begin{};
        while (begin < value.size()) {
            auto const separator{value.find("::", begin)};
            auto const end{separator == std::string_view::npos ? value.size() : separator};
            require_identifier(value.substr(begin, end - begin), context);
            if (separator == std::string_view::npos) {
                return;
            }
            begin = separator + 2;
        }
        throw std::invalid_argument{context +
                                    " must be a qualified C++ identifier: " + std::string{value}};
    }

    void require_unique_names(std::vector<std::string> const& names, std::string const& context) {
        std::set<std::string> unique;
        for (auto const& name : names) {
            require_value(name, context);
            if (!unique.insert(name).second) {
                throw std::invalid_argument{context + " contains duplicate name: " + name};
            }
        }
    }

    void validate_type(TypeRefId reference, TypeRegistry const& types, std::string const& context) {
        auto const type{reference};
        require_value(text(declarations_.type_refs.name[type]), context + " type");
        if (declarations_.type_refs.nested[type].has_value()) {
            require_value(*declarations_.type_refs.nested[type], context + " nested type");
        }
        static_cast<void>(resolved_type(reference, types));
    }

    void validate_semantic_relation(lispb::codegen::SemanticRelationId const& relationship,
                                    TypeRegistry const& types,
                                    std::string const& context) {
        validate_type(declarations_.semantic_relations.target[relationship], types, context);
        auto const offset_relation{declarations_.semantic_relations.kind[relationship] ==
                                   SemanticRelationKind::offset_into};
        if (offset_relation != declarations_.semantic_relations.unit[relationship].has_value()) {
            throw std::invalid_argument{context + (offset_relation
                                                       ? " 'offset_into' requires an explicit unit"
                                                       : " unit is only valid for 'offset_into'")};
        }
    }

    void validate_type_dependency(CppTypes const& types,
                                  TypeDependencyId const dependency,
                                  std::string const& context) {
        require_value(std::string{types.dependency_spelling(dependency)}, context + " spelling");
        if (auto const header{types.dependency_header(dependency)}) {
            require_value(std::string{*header}, context + " header");
        }
    }

    void validate_export_specifier(lispb::StringId const value, std::string const& context) {
        if (value.value != 0) {
            require_identifier(value, context);
        }
    }

    void validate_dependency(lispb::StringId key,
                             TypeRegistry const& types,
                             std::string const& context) {
        require_non_blank_value(key, context + " dependency");
        auto const found{types.find(view(key))};
        if (!found ||
            types.cpp_types.dependencies(types.registered.cpp_type[types.registered.index(*found)])
                .empty()) {
            throw std::invalid_argument{context + " has unknown dependency: " + text(key)};
        }
    }

    auto parameter_type_names(SchemaRange<ParameterId> parameters,
                              TypeRegistry const& types,
                              std::string const& context) -> std::vector<std::string> {
        std::vector<std::string> names;
        std::vector<std::string> type_names;
        bool has_default{};
        for (auto const parameter_id : declarations_.items(parameters)) {
            auto const parameter{parameter_id};
            names.push_back(text(declarations_.parameters.name[parameter]));
            require_identifier(text(declarations_.parameters.name[parameter]),
                               context + " parameter name");
            validate_type(declarations_.parameters.type[parameter],
                          types,
                          context + " parameter '" +
                              text(declarations_.parameters.name[parameter]) + "'");
            type_names.push_back(std::string{types.cpp_types.spelling(
                resolved_type(declarations_.parameters.type[parameter], types))});
            if (declarations_.parameters.default_value[parameter].has_value()) {
                require_non_blank_value(*declarations_.parameters.default_value[parameter],
                                        context + " parameter '" +
                                            text(declarations_.parameters.name[parameter]) +
                                            "' default");
                has_default = true;
            } else if (has_default) {
                throw std::invalid_argument{context +
                                            " has a non-defaulted parameter after a defaulted one"};
            }
        }
        require_unique_names(names, context + " parameters");
        return type_names;
    }

    auto signature(lispb::StringId name,
                   std::vector<std::string> const& parameter_types,
                   bool const is_const) -> std::string {
        std::string result{text(name) + "("};
        for (auto const& type : parameter_types) {
            result += type + ";";
        }
        return result + ")" + (is_const ? " const" : "");
    }

    void validate_function(lispb::codegen::FunctionSchemaId const& function,
                           TypeRegistry const& types,
                           std::string const& context,
                           std::set<std::string>& signatures,
                           bool const soa_owner = false) {
        auto const& functions{declarations_.functions};
        auto const row{functions.index(function)};
        if (auto const columns{functions.append_columns[row]}) {
            if (!soa_owner || functions.is_static[row] || functions.is_const[row] ||
                functions.is_constexpr[row] || !functions.body_lines[row].empty()) {
                throw std::invalid_argument{context + " append-columns requires a mutable owner "
                                                      "function without a body or constexpr"};
            }
            if (columns->size() != functions.parameters[row].size()) {
                throw std::invalid_argument{context +
                                            " append-columns requires one column per parameter"};
            }
            require_unique_names(*columns, context + " append columns");
            for (auto const parameter : declarations_.items(functions.parameters[row])) {
                auto const type{declarations_.parameters.type[parameter]};
                auto const occurrence{graph_.resolved(module_, type)};
                auto const& occurrences{graph_.occurrences()};
                auto const form{occurrences.physical_form[occurrences.index(occurrence)]};
                if (form == PhysicalTypeForm::lvalue_reference ||
                    form == PhysicalTypeForm::rvalue_reference) {
                    throw std::invalid_argument{
                        context +
                        " append-columns parameters must be passed by value before growth"};
                }
            }
        }
        if (declarations_.functions.is_constexpr[function] &&
            (declarations_.functions.body_lines[function].empty() ||
             declarations_.functions.definition_in_source[function])) {
            throw std::invalid_argument{context +
                                        " constexpr function requires a generated header body"};
        }
        if (declarations_.functions.is_static[function] &&
            declarations_.functions.is_const[function]) {
            throw std::invalid_argument{context + " must not be both static and const"};
        }
        if (declarations_.functions.is_inline[function] &&
            declarations_.functions.definition_in_source[function]) {
            throw std::invalid_argument{context +
                                        " must not be both inline and defined in the source"};
        }
        if (declarations_.functions.template_parameters[function] &&
            declarations_.functions.definition_in_source[function]) {
            throw std::invalid_argument{context +
                                        " function templates must be defined in the header"};
        }
        if (declarations_.functions.template_parameters[function]) {
            require_non_blank_value(*declarations_.functions.template_parameters[function],
                                    context + " template parameters");
        }
        if (declarations_.functions.requires_clause[function]) {
            require_non_blank_value(*declarations_.functions.requires_clause[function],
                                    context + " requires clause");
        }

        validate_type(declarations_.functions.return_type[function], types, context + " return");
        if (declarations_.functions.trailing_return_type[function]) {
            if (view(declarations_.type_refs.name[declarations_.functions.return_type[function]]) !=
                    "auto" ||
                !view(declarations_.type_refs.suffix[declarations_.functions.return_type[function]])
                     .empty() ||
                declarations_.type_refs.nested[declarations_.functions.return_type[function]]) {
                throw std::invalid_argument{context +
                                            " trailing return type requires an auto return"};
            }
            validate_type(*declarations_.functions.trailing_return_type[function],
                          types,
                          context + " trailing return");
        }
        auto const& effective_return{declarations_.functions.trailing_return_type[function]
                                         ? *declarations_.functions.trailing_return_type[function]
                                         : declarations_.functions.return_type[function]};
        auto const result{graph_.resolved(module_, effective_return)};
        auto const& occurrences{graph_.occurrences()};
        auto const occurrence_row{occurrences.index(result)};
        if (functions.append_columns[row] &&
            types.cpp_types.strings().view(occurrences.object_spelling[occurrence_row]) == "void") {
            throw std::invalid_argument{context +
                                        " append-columns must return a row index or row ID"};
        }
        if (declarations_.functions.is_nodiscard[function] &&
            occurrences.physical_form[occurrence_row] == PhysicalTypeForm::value &&
            types.cpp_types.strings().view(occurrences.object_spelling[occurrence_row]) == "void") {
            throw std::invalid_argument{context + " nodiscard function must not return void"};
        }

        if (!signatures
                 .insert(
                     signature(declarations_.functions.name[function],
                               parameter_type_names(
                                   declarations_.functions.parameters[function], types, context),
                               declarations_.functions.is_const[function]))
                 .second) {
            throw std::invalid_argument{"Duplicate " + context + " signature"};
        }
        for (auto const dependency :
             declarations_.items(declarations_.functions.dependencies[function])) {
            validate_dependency(dependency, types, context);
        }
    }

    void validate_settings(lispb::codegen::ModuleSettingsId const& settings) {
        require_value(text(declarations_.module_settings_store.name[settings]), "Module name");
        if (text(declarations_.module_settings_store.header[settings]).empty()) {
            throw std::invalid_argument{"Module '" +
                                        text(declarations_.module_settings_store.name[settings]) +
                                        "' header must not be empty"};
        }
        if (declarations_.module_settings_store.source[settings].has_value() &&
            view(*declarations_.module_settings_store.source[settings]).empty()) {
            throw std::invalid_argument{"Module '" +
                                        text(declarations_.module_settings_store.name[settings]) +
                                        "' source must not be empty"};
        }
        if (declarations_.module_settings_store.header_include[settings].has_value()) {
            require_value(*declarations_.module_settings_store.header_include[settings],
                          "Module '" + text(declarations_.module_settings_store.name[settings]) +
                              "' header include");
        }
        if (declarations_.module_settings_store.namespace_name[settings].has_value()) {
            require_qualified_identifier(
                *declarations_.module_settings_store.namespace_name[settings],
                "Module '" + text(declarations_.module_settings_store.name[settings]) +
                    "' namespace");
        }
        require_unique_names(declarations_.module_settings_store.include_order[settings],
                             "Module '" + text(declarations_.module_settings_store.name[settings]) +
                                 "' include order");
    }

    auto enum_storage_domain(std::string_view const spelling) -> std::optional<EnumStorageDomain> {
        if (auto const bit_width{packed_unsigned_width(spelling)}) {
            return EnumStorageDomain{static_cast<std::uint32_t>(*bit_width), true};
        }
        if (auto const bit_width{packed_signed_width(spelling)}) {
            return EnumStorageDomain{static_cast<std::uint32_t>(*bit_width), false};
        }
        if (spelling == "int") {
            return EnumStorageDomain{
                static_cast<std::uint32_t>(std::numeric_limits<int>::digits + 1), false};
        }
        return std::nullopt;
    }

    auto enum_code_fits(lispb::schema::EnumCode const code, EnumStorageDomain const storage)
        -> bool {
        if (storage.is_unsigned) {
            if (code.negative) {
                return false;
            }
            return storage.bit_width == 64 ||
                   code.magnitude < (std::uint64_t{1} << storage.bit_width);
        }

        auto const sign_magnitude{std::uint64_t{1} << (storage.bit_width - 1)};
        return code.negative ? code.magnitude <= sign_magnitude : code.magnitude < sign_magnitude;
    }

    void validate_enum(lispb::codegen::DeclarationHandle const& schema,
                       lispb::codegen::ModuleHandle const& module,
                       TypeRegistry const& types) {
        require_identifier(text(declarations_.enums.name[schema]), "Enum name");

        auto const& domains{graph_.enum_domains()};
        auto const domain{domains.find(schema)};
        auto const domain_row{domains.index(domain)};
        std::optional<EnumStorageDomain> storage_domain;
        if (declarations_.enums.underlying_type[schema].has_value()) {
            validate_type(*declarations_.enums.underlying_type[schema],
                          types,
                          "Enum '" + text(declarations_.enums.name[schema]) + "' underlying");
            auto const underlying{
                resolved_type(*declarations_.enums.underlying_type[schema], types)};
            storage_domain = enum_storage_domain(std::string{types.cpp_types.spelling(underlying)});
        } else {
            auto const derived{lispb::schema::derive_enum_storage_requirement(
                domains, domain, declarations_.enums.bit_width[schema])};
            if (!derived.has_value()) {
                throw std::invalid_argument{
                    "Enum '" + text(declarations_.enums.name[schema]) +
                    "' cannot derive C++ backing storage because its semantic signedness or width "
                    "is unknown"};
            }
            storage_domain = EnumStorageDomain{.bit_width = derived->bit_width,
                                               .is_unsigned = !derived->signedness};
        }
        if (declarations_.enums.signedness[schema] != schema::EnumSignedness::unspecified) {
            auto const issue_count{domains.issue_count(domain)};
            for (std::uint32_t issue{}; issue < issue_count; ++issue) {
                if (domains.issue_severity(domain, issue) ==
                    schema::EnumDomainIssueSeverity::error) {
                    throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                                "': " + domains.issue_message(domain, issue)};
                }
            }
        }
        if (declarations_.enums.bit_width[schema].has_value()) {
            if (*declarations_.enums.bit_width[schema] == 0 ||
                *declarations_.enums.bit_width[schema] > 64) {
                throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                            "' bit width must be between 1 and 64 bits"};
            }
            if (domains.minimum_required_bits[domain_row].has_value() &&
                *domains.minimum_required_bits[domain_row] >
                    *declarations_.enums.bit_width[schema]) {
                throw std::invalid_argument{
                    std::format("Enum '{}' known value domain requires at least {} bits but its "
                                "semantic bit width is {}",
                                text(declarations_.enums.name[schema]),
                                *domains.minimum_required_bits[domain_row],
                                *declarations_.enums.bit_width[schema])};
            }
        }
        validate_export_specifier(declarations_.enums.export_specifier[schema],
                                  "Enum '" + text(declarations_.enums.name[schema]) +
                                      "' export specifier");
        if (declarations_.enums.unreal_projection[schema].has_value()) {
            auto const projection{*declarations_.enums.unreal_projection[schema]};
            require_identifier(text(declarations_.enum_unreal_projections.name[projection]),
                               "Enum '" + text(declarations_.enums.name[schema]) +
                                   "' Unreal projection name");
            require_value(text(declarations_.enum_unreal_projections.header[projection]),
                          "Enum '" + text(declarations_.enums.name[schema]) +
                              "' Unreal projection header");
            require_value(text(declarations_.enum_unreal_projections.header_include[projection]),
                          "Enum '" + text(declarations_.enums.name[schema]) +
                              "' Unreal projection header include");
            require_value(text(declarations_.enum_unreal_projections.conversion_header[projection]),
                          "Enum '" + text(declarations_.enums.name[schema]) +
                              "' Unreal projection conversion header");
            require_value(declarations_.enum_unreal_projections.native_header_include[projection],
                          "Enum '" + text(declarations_.enums.name[schema]) +
                              "' native enum header include");
            if (!declarations_.enums.native_api[schema] ||
                declarations_.enums.reflection[schema] != EnumReflection::none) {
                throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                            "' Unreal projection requires a native plain enum"};
            }
            if (declarations_.enum_unreal_projections.reflection[projection] ==
                EnumReflection::none) {
                throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                            "' Unreal projection must be reflected"};
            }
        }
        if (!declarations_.enums.native_api[schema] &&
            !declarations_.enums.helpers[schema].empty()) {
            throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                        "' helpers require native-api"};
        }
        std::set<NativeEnumHelper> helpers;
        for (auto const helper : declarations_.items(declarations_.enums.helpers[schema])) {
            if (!try_parse_serialized_native_enum_helper(to_serialized_string(helper)) ||
                !helpers.insert(helper).second) {
                throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                            "' has an invalid or duplicate helper"};
            }
        }
        if (declarations_.enums.native_api[schema] &&
            declarations_.enums.reflection[schema] != EnumReflection::none) {
            throw std::invalid_argument{"Native enum '" + text(declarations_.enums.name[schema]) +
                                        "' cannot use Unreal reflection"};
        }
        if (declarations_.enums.native_api[schema] &&
            (!declarations_.enums.conversions[schema].empty() ||
             (declarations_.enums.export_specifier[schema].value != 0))) {
            throw std::invalid_argument{"Native enum '" + text(declarations_.enums.name[schema]) +
                                        "' cannot use Unreal enum generation options"};
        }
        if (declarations_.enums.reflection[schema] != EnumReflection::none &&
            declarations_.module_settings_store
                .namespace_name[declarations_.normal_modules.settings[module]]
                .has_value()) {
            throw std::invalid_argument{"Reflected enum '" +
                                        text(declarations_.enums.name[schema]) +
                                        "' must be declared at global scope"};
        }
        if (declarations_.enums.values[schema].empty()) {
            throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                        "' must have values"};
        }
        std::vector<std::string> value_names;
        std::vector<std::string> serialized_names;
        auto const has_serialized_conversion{
            helpers.contains(NativeEnumHelper::to_serialized_string) ||
            helpers.contains(NativeEnumHelper::try_parse_serialized) ||
            std::find(declarations_.items(declarations_.enums.conversions[schema]).begin(),
                      declarations_.items(declarations_.enums.conversions[schema]).end(),
                      EnumConversion::lex_to_serialized_string) !=
                declarations_.items(declarations_.enums.conversions[schema]).end() ||
            std::find(declarations_.items(declarations_.enums.conversions[schema]).begin(),
                      declarations_.items(declarations_.enums.conversions[schema]).end(),
                      EnumConversion::try_parse_serialized) !=
                declarations_.items(declarations_.enums.conversions[schema]).end()};
        for (std::size_t index{}; index < declarations_.enums.values[schema].size(); ++index) {
            auto const value{declarations_.items(declarations_.enums.values[schema])[index]};
            require_identifier(text(declarations_.enumerators.name[value]),
                               "Enum '" + text(declarations_.enums.name[schema]) + "' value name");
            value_names.push_back(text(declarations_.enumerators.name[value]));
            if (declarations_.enumerators.initializer[value].has_value()) {
                require_value(*declarations_.enumerators.initializer[value],
                              "Enum '" + text(declarations_.enums.name[schema]) + "' value '" +
                                  text(declarations_.enumerators.name[value]) + "' initializer");
            }
            auto const code{domains.code(domain, index)};
            if (storage_domain.has_value() && code.has_value() &&
                !enum_code_fits(*code, *storage_domain)) {
                throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                            "' value '" +
                                            text(declarations_.enumerators.name[value]) +
                                            "' does not fit its underlying type"};
            }
            if (declarations_.enumerators.display_name[value].has_value()) {
                require_value(*declarations_.enumerators.display_name[value],
                              "Enum '" + text(declarations_.enums.name[schema]) + "' value '" +
                                  text(declarations_.enumerators.name[value]) + "' display name");
            }
            if (declarations_.enumerators.serialized_name[value].has_value()) {
                require_value(*declarations_.enumerators.serialized_name[value],
                              "Enum '" + text(declarations_.enums.name[schema]) + "' value '" +
                                  text(declarations_.enumerators.name[value]) +
                                  "' serialized name");
                serialized_names.push_back(text(*declarations_.enumerators.serialized_name[value]));
            } else if (has_serialized_conversion &&
                       (!declarations_.enums.count[schema].has_value() ||
                        declarations_.enumerators.name[value] !=
                            *declarations_.enums.count[schema])) {
                throw std::invalid_argument{
                    "Enum '" + text(declarations_.enums.name[schema]) + "' value '" +
                    text(declarations_.enumerators.name[value]) + "' requires a serialized name"};
            }
            if (declarations_.enumerators.hidden[value] &&
                declarations_.enums.reflection[schema] == EnumReflection::none &&
                !declarations_.enums.unreal_projection[schema].has_value() &&
                (!declarations_.enums.count[schema].has_value() ||
                 declarations_.enumerators.name[value] != *declarations_.enums.count[schema])) {
                throw std::invalid_argument{
                    "Plain enum '" + text(declarations_.enums.name[schema]) + "' value '" +
                    text(declarations_.enumerators.name[value]) + "' cannot be hidden"};
            }
        }
        require_unique_names(value_names,
                             "Enum '" + text(declarations_.enums.name[schema]) + "' values");
        require_unique_names(serialized_names,
                             "Enum '" + text(declarations_.enums.name[schema]) +
                                 "' serialized names");

        auto count_value{declarations_.items(declarations_.enums.values[schema]).end()};
        if (declarations_.enums.count[schema].has_value()) {
            require_identifier(*declarations_.enums.count[schema],
                               "Enum '" + text(declarations_.enums.name[schema]) + "' count");
            count_value =
                std::find_if(declarations_.items(declarations_.enums.values[schema]).begin(),
                             declarations_.items(declarations_.enums.values[schema]).end(),
                             [&](auto const value) {
                                 return declarations_.enumerators.name[value] ==
                                      *declarations_.enums.count[schema];
                             });
            if (count_value == declarations_.items(declarations_.enums.values[schema]).end()) {
                throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                            "' count '" + text(*declarations_.enums.count[schema]) +
                                            "' does not name an enum value"};
            }
            if (std::next(count_value) !=
                declarations_.items(declarations_.enums.values[schema]).end()) {
                throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                            "' count must be the final enum value"};
            }
            if (count_value == declarations_.items(declarations_.enums.values[schema]).begin()) {
                throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                            "' count must follow at least one value"};
            }
        }
        if (declarations_.enums.enum_array[schema] ||
            (declarations_.enums.unreal_projection[schema].has_value() &&
             declarations_.enum_unreal_projections
                 .enum_array[*declarations_.enums.unreal_projection[schema]])) {
            if (declarations_.enums.count[schema].has_value()) {
                throw std::invalid_argument{
                    "Enum-array enum '" + text(declarations_.enums.name[schema]) +
                    "' must use its generated count instead of a count sentinel"};
            }

            // Certify direct indexing before emitting the count specialization.
            auto const values{domains.values(domain)};
            auto const value_count{values.size()};
            for (std::size_t index{}; index < value_count; ++index) {
                auto const value{values[index]};
                auto const code{domains.code(domain, index)};
                auto const value_row{declarations_.enumerators.index(value)};
                auto const name{declarations_.enumerators.name[value_row]};
                if (!code.has_value() || code->negative || code->magnitude != index) {
                    throw std::invalid_argument{std::format(
                        "Enum-array enum '{}' value '{}' must have the zero-based contiguous code "
                        "{}",
                        text(declarations_.enums.name[schema]),
                        text(name),
                        index)};
                }
                if (declarations_.enumerators.hidden[value_row]) {
                    throw std::invalid_argument{"Enum-array enum '" +
                                                text(declarations_.enums.name[schema]) +
                                                "' value '" + text(name) + "' must not be hidden"};
                }
            }
        }

        std::set<EnumConversion> conversions;
        for (auto const conversion : declarations_.items(declarations_.enums.conversions[schema])) {
            if (!conversions.insert(conversion).second) {
                throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                            "' contains duplicate conversion"};
            }
        }
        if (!declarations_.enums.conversions[schema].empty() &&
            !declarations_.module_settings_store
                 .source[declarations_.normal_modules.settings[module]]
                 .has_value()) {
            throw std::invalid_argument{"Enum '" + text(declarations_.enums.name[schema]) +
                                        "' conversions require a source output"};
        }
    }

    void validate_packed_value(lispb::codegen::DeclarationHandle const& value,
                               TypeRegistry const& types) {
        auto const& integer_scalars{graph_.definitions().integer_scalars};
        auto const context{"Packed value '" + text(declarations_.packed_values.name[value]) + "'"};
        require_identifier(text(declarations_.packed_values.name[value]), "Packed value name");

        validate_export_specifier(declarations_.packed_values.export_specifier[value],
                                  context + " export specifier");
        validate_type(declarations_.packed_values.storage_type[value], types, context + " storage");
        auto const storage{resolved_type(declarations_.packed_values.storage_type[value], types)};
        auto const storage_width{
            packed_unsigned_width(std::string{types.cpp_types.spelling(storage)})};
        if (!storage_width.has_value()) {
            throw std::invalid_argument{context + " has unsupported storage type: " +
                                        std::string{types.cpp_types.spelling(storage)}};
        }
        if (declarations_.packed_values.segments[value].empty()) {
            throw std::invalid_argument{context + " must have segments"};
        }
        if (!std::ranges::any_of(
                declarations_.items(declarations_.packed_values.segments[value]),
                [](auto const& segment) { return segment.kind() == PackedSegmentKind::field; })) {
            throw std::invalid_argument{context + " must have at least one semantic field"};
        }
        if (declarations_.packed_values.invalid_value[value].has_value() && *storage_width < 64 &&
            *declarations_.packed_values.invalid_value[value] >=
                (std::uint64_t{1} << *storage_width)) {
            throw std::invalid_argument{std::format(
                "{} invalid value does not fit in {}-bit storage", context, *storage_width)};
        }

        std::set<std::string> generated_names{text(declarations_.packed_values.name[value]),
                                              "raw_value",
                                              "storage_type",
                                              "from_raw",
                                              "is_valid"};
        if (declarations_.packed_values.invalid_value[value].has_value()) {
            generated_names.insert("invalid_value");
        }
        int used_bits{};
        std::set<std::string> segment_names;
        for (auto const segment :
             declarations_.items(declarations_.packed_values.segments[value])) {
            auto const& segment_name{text(segment.kind() == PackedSegmentKind::field
                                              ? declarations_.packed_fields.name[segment]
                                              : declarations_.packed_reserved_bits.name[segment])};
            auto const segment_context{context + " segment '" + segment_name + "'"};
            require_identifier(segment_name, context + " segment name");
            if (!segment_names.insert(segment_name).second) {
                throw std::invalid_argument{context +
                                            " has duplicate segment name: " + segment_name};
            }
            auto const field{((segment).kind() == lispb::codegen::PackedSegmentKind::field
                                  ? std::optional{segment}
                                  : std::nullopt)};
            auto const scalar{field.has_value()
                                  ? find_packed_integer_domain(*field, graph_, module_)
                                  : std::nullopt};
            auto const quantized{
                field.has_value() ? find_linear_quantized(
                                        declarations_.packed_fields.type[*field], graph_, module_)
                                  : std::nullopt};
            auto const fixed{
                field.has_value()
                    ? find_fixed_point(declarations_.packed_fields.type[*field], graph_, module_)
                    : std::nullopt};
            auto const mini{
                field.has_value()
                    ? find_mini_float(declarations_.packed_fields.type[*field], graph_, module_)
                    : std::nullopt};
            auto const segment_bits{
                field.has_value() ? derive_packed_field_width(*field, manifest_, graph_, module_)
                                  : std::optional<int>{static_cast<int>(
                                        declarations_.packed_reserved_bits.bits[segment])}};
            if (!segment_bits.has_value()) {
                if (scalar.has_value()) {
                    throw std::invalid_argument{
                        segment_context +
                        " integer-scalar domain does not fit the supported 1-64-bit range"};
                }
                if (field.has_value() &&
                    declarations_.packed_fields.kind[*field] == PackedFieldKind::linear_quantized) {
                    throw std::invalid_argument{
                        segment_context +
                        " linear-quantized type does not resolve to a supported representation"};
                }
                if (field.has_value() &&
                    declarations_.packed_fields.kind[*field] == PackedFieldKind::fixed_point) {
                    throw std::invalid_argument{
                        segment_context +
                        " fixed-point type does not resolve to a supported representation"};
                }
                if (field.has_value() &&
                    declarations_.packed_fields.kind[*field] == PackedFieldKind::mini_float) {
                    throw std::invalid_argument{
                        segment_context +
                        " mini-float type does not resolve to a supported representation"};
                }
                if (field.has_value() &&
                    (declarations_.packed_fields.kind[*field] == PackedFieldKind::signed_integer ||
                     declarations_.packed_fields.kind[*field] ==
                         PackedFieldKind::unsigned_integer)) {
                    if (!declarations_.packed_fields.minimum_value[*field].has_value() ||
                        !declarations_.packed_fields.maximum_value[*field].has_value()) {
                        throw std::invalid_argument{
                            segment_context +
                            " integer auto width requires a semantic minimum and maximum"};
                    }
                    if (packed_integer_less(*declarations_.packed_fields.maximum_value[*field],
                                            *declarations_.packed_fields.minimum_value[*field])) {
                        throw std::invalid_argument{segment_context +
                                                    " semantic range minimum exceeds maximum"};
                    }
                    throw std::invalid_argument{
                        segment_context +
                        " semantic range or named codes do not fit the supported 1-64-bit domain"};
                }
                throw std::invalid_argument{
                    segment_context +
                    " auto width requires a known enum domain or integer semantic range"};
            }
            if (*segment_bits <= 0) {
                throw std::invalid_argument{segment_context + " bits must be greater than zero"};
            }
            if (*segment_bits > *storage_width - used_bits) {
                throw std::invalid_argument{
                    std::format("{} does not fit in {}-bit storage: requires {} bits, {} already "
                                "used, {} remaining",
                                segment_context,
                                *storage_width,
                                *segment_bits,
                                used_bits,
                                *storage_width - used_bits)};
            }
            used_bits += *segment_bits;

            if (!field) {
                continue;
            }
            auto const field_bits{*segment_bits};
            auto const field_context{context + " field '" +
                                     text(declarations_.packed_fields.name[*field]) + "'"};
            require_identifier(text(declarations_.packed_fields.name[*field]),
                               context + " field name");
            validate_type(declarations_.packed_fields.type[*field], types, field_context);
            if (declarations_.packed_fields.relationship[*field].has_value()) {
                if (quantized.has_value() || fixed.has_value() || mini.has_value()) {
                    throw std::invalid_argument{
                        field_context +
                        (quantized.has_value()
                             ? " linear-quantized representation owns its semantic relationship "
                               "through its source scalar"
                         : fixed.has_value()
                             ? " fixed-point representation cannot carry an integer-field "
                               "semantic relationship"
                             : " mini-float representation cannot carry an integer-field "
                               "semantic relationship")};
                }
                validate_semantic_relation(*declarations_.packed_fields.relationship[*field],
                                           types,
                                           field_context + " relationship");
                auto const relation_kind{
                    declarations_.semantic_relations
                        .kind[*declarations_.packed_fields.relationship[*field]]};
                auto const requires_integer{relation_kind == SemanticRelationKind::index_into ||
                                            relation_kind == SemanticRelationKind::count_of ||
                                            relation_kind == SemanticRelationKind::offset_into};
                if (requires_integer &&
                    declarations_.packed_fields.kind[*field] != PackedFieldKind::unsigned_integer) {
                    throw std::invalid_argument{
                        std::format("{} relationship '{}' requires an unsigned integer field",
                                    field_context,
                                    to_serialized_string(relation_kind))};
                }
                if (relation_kind == SemanticRelationKind::discriminates &&
                    declarations_.packed_fields.kind[*field] != PackedFieldKind::enumeration) {
                    throw std::invalid_argument{
                        field_context + " relationship 'discriminates' requires an enum field"};
                }
            }

            auto const field_type{resolved_type(declarations_.packed_fields.type[*field], types)};
            if ((scalar.has_value() || quantized.has_value() || fixed.has_value() ||
                 mini.has_value()) &&
                (declarations_.packed_fields.minimum_value[*field].has_value() ||
                 declarations_.packed_fields.maximum_value[*field].has_value() ||
                 !declarations_.packed_fields.named_codes[*field].empty())) {
                throw std::invalid_argument{
                    field_context +
                    (scalar.has_value()
                         ? " integer-scalar type owns its semantic range and named codes"
                     : quantized.has_value()
                         ? " linear-quantized representation owns its semantic domain and code "
                           "space"
                     : fixed.has_value()
                         ? " fixed-point representation owns its raw and numerical domains"
                         : " mini-float representation owns its encoded code space")};
            }
            if (quantized.has_value()) {
                if (declarations_.packed_fields.kind[*field] != PackedFieldKind::linear_quantized) {
                    throw std::invalid_argument{
                        field_context + " kind must be 'linear-quantized' for its representation "
                                        "type"};
                }
                if (declarations_.packed_fields.bits[*field].has_value() &&
                    *declarations_.packed_fields.bits[*field] !=
                        static_cast<int>(declarations_.linear_quantizeds.bit_width[*quantized])) {
                    throw std::invalid_argument{std::format(
                        "{} width must equal its linear-quantized representation's {}-bit encoding",
                        field_context,
                        declarations_.linear_quantizeds.bit_width[*quantized])};
                }
                if (declarations_.packed_fields.range_helper[*field]) {
                    throw std::invalid_argument{
                        field_context +
                        " linear-quantized representation cannot use an integer range helper"};
                }
            } else if (declarations_.packed_fields.kind[*field] ==
                       PackedFieldKind::linear_quantized) {
                throw std::invalid_argument{
                    field_context +
                    " ':kind linear-quantized' type must resolve to a linear-quantized "
                    "representation"};
            } else if (fixed.has_value()) {
                if (declarations_.packed_fields.kind[*field] != PackedFieldKind::fixed_point) {
                    throw std::invalid_argument{
                        field_context + " kind must be 'fixed-point' for its representation type"};
                }
                if (declarations_.packed_fields.bits[*field].has_value() &&
                    *declarations_.packed_fields.bits[*field] !=
                        static_cast<int>(declarations_.fixed_points.total_bits[*fixed])) {
                    throw std::invalid_argument{std::format(
                        "{} width must equal its fixed-point representation's {}-bit encoding",
                        field_context,
                        declarations_.fixed_points.total_bits[*fixed])};
                }
                if (declarations_.packed_fields.range_helper[*field]) {
                    throw std::invalid_argument{
                        field_context +
                        " fixed-point representation cannot use an integer range helper"};
                }
            } else if (declarations_.packed_fields.kind[*field] == PackedFieldKind::fixed_point) {
                throw std::invalid_argument{
                    field_context +
                    " ':kind fixed-point' type must resolve to a fixed-point representation"};
            } else if (mini.has_value()) {
                auto const total_bits{declarations_.mini_floats.sign_bits[*mini] +
                                      declarations_.mini_floats.exponent_bits[*mini] +
                                      declarations_.mini_floats.significand_bits[*mini]};
                if (declarations_.packed_fields.kind[*field] != PackedFieldKind::mini_float) {
                    throw std::invalid_argument{
                        field_context + " kind must be 'mini-float' for its representation type"};
                }
                if (declarations_.packed_fields.bits[*field].has_value() &&
                    *declarations_.packed_fields.bits[*field] != static_cast<int>(total_bits)) {
                    throw std::invalid_argument{std::format(
                        "{} width must equal its mini-float representation's {}-bit encoding",
                        field_context,
                        total_bits)};
                }
                if (declarations_.packed_fields.range_helper[*field]) {
                    throw std::invalid_argument{
                        field_context +
                        " mini-float representation cannot use an integer range helper"};
                }
            } else if (declarations_.packed_fields.kind[*field] == PackedFieldKind::mini_float) {
                throw std::invalid_argument{
                    field_context +
                    " ':kind mini-float' type must resolve to a mini-float representation"};
            } else if (scalar.has_value()) {
                auto const expected_kind{integer_scalars.signedness[integer_scalars.index(*scalar)]
                                             ? PackedFieldKind::signed_integer
                                             : PackedFieldKind::unsigned_integer};
                if (declarations_.packed_fields.kind[*field] != expected_kind) {
                    throw std::invalid_argument{std::format(
                        "{} kind must match its {} integer-scalar type",
                        field_context,
                        integer_scalars.signedness[integer_scalars.index(*scalar)] ? "signed"
                                                                                   : "unsigned")};
                }
                auto const scalar_width{
                    static_cast<int>(integer_scalars.bit_width[integer_scalars.index(*scalar)])};
                if (field_bits < scalar_width) {
                    throw std::invalid_argument{std::format(
                        "{} width is smaller than the integer scalar's {}-bit semantic domain",
                        field_context,
                        scalar_width)};
                }
            } else if (declarations_.packed_fields.kind[*field] ==
                       PackedFieldKind::unsigned_integer) {
                if (std::string{types.cpp_types.spelling(field_type)} == "bool") {
                    if (field_bits != 1) {
                        throw std::invalid_argument{field_context + " bool type must use one bit"};
                    }
                } else {
                    auto const field_width{
                        packed_unsigned_width(std::string{types.cpp_types.spelling(field_type)})};
                    if (!field_width.has_value()) {
                        throw std::invalid_argument{field_context +
                                                    " must use a supported unsigned integer type, "
                                                    "bool, or ':kind enum'"};
                    }
                    if (field_bits > *field_width) {
                        throw std::invalid_argument{field_context + " width exceeds its type"};
                    }
                }
            } else if (declarations_.packed_fields.kind[*field] ==
                       PackedFieldKind::signed_integer) {
                auto const field_width{
                    packed_signed_width(std::string{types.cpp_types.spelling(field_type)})};
                if (!field_width.has_value()) {
                    throw std::invalid_argument{field_context +
                                                " must use a supported signed integer type"};
                }
                if (field_bits > *field_width) {
                    throw std::invalid_argument{field_context + " width exceeds its signed type"};
                }
            } else if (packed_unsigned_width(std::string{types.cpp_types.spelling(field_type)})
                           .has_value() ||
                       packed_signed_width(std::string{types.cpp_types.spelling(field_type)})
                           .has_value() ||
                       std::string{types.cpp_types.spelling(field_type)} == "bool") {
                throw std::invalid_argument{field_context +
                                            " ':kind enum' type must not be an integer or bool"};
            } else if (auto const enum_schema{find_packed_enum(
                           declarations_.packed_fields.type[*field], graph_, module_)}) {
                auto const& domains{graph_.enum_domains()};
                auto const domain{domains.find(*enum_schema)};
                auto const domain_row{domains.index(domain)};
                auto const semantic_width{declarations_.enums.bit_width[*enum_schema].has_value()
                                              ? declarations_.enums.bit_width[*enum_schema]
                                              : domains.minimum_required_bits[domain_row]};
                if (semantic_width.has_value() && field_bits < static_cast<int>(*semantic_width)) {
                    throw std::invalid_argument{
                        std::format("{} width is smaller than the enum's {}-bit semantic domain",
                                    field_context,
                                    *semantic_width)};
                }
                std::optional<int> underlying_width;
                if (declarations_.enums.underlying_type[*enum_schema].has_value()) {
                    auto const& occurrences{graph_.occurrences()};
                    auto const enum_occurrence{
                        graph_.resolved(module_, declarations_.packed_fields.type[*field])};
                    auto const enum_module{
                        graph_.module(occurrences.type[occurrences.index(enum_occurrence)])};
                    auto const underlying_occurrence{graph_.resolved(
                        enum_module, *declarations_.enums.underlying_type[*enum_schema])};
                    auto const underlying{
                        occurrences.cpp_type[occurrences.index(underlying_occurrence)]};
                    underlying_width =
                        packed_unsigned_width(std::string{types.cpp_types.spelling(underlying)});
                } else if (auto const derived{lispb::schema::derive_enum_storage_requirement(
                               domains, domain, declarations_.enums.bit_width[*enum_schema])};
                           derived.has_value() && !derived->signedness) {
                    underlying_width = static_cast<int>(derived->bit_width);
                }
                if (!underlying_width.has_value()) {
                    throw std::invalid_argument{field_context +
                                                " enum must have an unsigned fixed-width "
                                                "underlying type"};
                }
                if (field_bits > *underlying_width) {
                    throw std::invalid_argument{field_context +
                                                " width exceeds its enum underlying type"};
                }
            }
            if (declarations_.packed_fields.range_helper[*field] &&
                (declarations_.packed_fields.kind[*field] != PackedFieldKind::unsigned_integer ||
                 std::string{types.cpp_types.spelling(field_type)} == "bool")) {
                throw std::invalid_argument{field_context +
                                            " range helper requires an unsigned integer field"};
            }
            if (!scalar.has_value() && !quantized.has_value() && !fixed.has_value()) {
                if (declarations_.packed_fields.minimum_value[*field].has_value() !=
                    declarations_.packed_fields.maximum_value[*field].has_value()) {
                    throw std::invalid_argument{
                        field_context + " semantic range requires both minimum and maximum"};
                }
                if (declarations_.packed_fields.minimum_value[*field].has_value()) {
                    if ((declarations_.packed_fields.kind[*field] !=
                             PackedFieldKind::unsigned_integer &&
                         declarations_.packed_fields.kind[*field] !=
                             PackedFieldKind::signed_integer) ||
                        std::string{types.cpp_types.spelling(field_type)} == "bool") {
                        throw std::invalid_argument{field_context +
                                                    " semantic range requires an integer field"};
                    }
                    if (packed_integer_less(*declarations_.packed_fields.maximum_value[*field],
                                            *declarations_.packed_fields.minimum_value[*field])) {
                        throw std::invalid_argument{field_context +
                                                    " semantic range minimum exceeds maximum"};
                    }
                    auto const minimum_fits{
                        declarations_.packed_fields.kind[*field] == PackedFieldKind::signed_integer
                            ? packed_integer_fits_signed(
                                  *declarations_.packed_fields.minimum_value[*field], field_bits)
                            : packed_integer_fits_unsigned(
                                  *declarations_.packed_fields.minimum_value[*field], field_bits)};
                    auto const maximum_fits{
                        declarations_.packed_fields.kind[*field] == PackedFieldKind::signed_integer
                            ? packed_integer_fits_signed(
                                  *declarations_.packed_fields.maximum_value[*field], field_bits)
                            : packed_integer_fits_unsigned(
                                  *declarations_.packed_fields.maximum_value[*field], field_bits)};
                    if (!minimum_fits || !maximum_fits) {
                        throw std::invalid_argument{field_context +
                                                    " semantic range does not fit its width"};
                    }
                }

                std::set<std::string> named_code_names;
                std::set<std::pair<bool, std::uint64_t>> named_code_values;
                for (auto const code_id :
                     declarations_.items(declarations_.packed_fields.named_codes[*field])) {
                    auto const code{code_id};
                    auto const code_context{field_context + " code '" +
                                            text(declarations_.packed_named_codes.name[code]) +
                                            "'"};
                    require_identifier(text(declarations_.packed_named_codes.name[code]),
                                       code_context + " name");
                    if (!named_code_names.insert(text(declarations_.packed_named_codes.name[code]))
                             .second) {
                        throw std::invalid_argument{
                            field_context + " has duplicate named code: " +
                            text(declarations_.packed_named_codes.name[code])};
                    }
                    if (!named_code_values
                             .emplace(declarations_.packed_named_codes.value[code].negative,
                                      declarations_.packed_named_codes.value[code].magnitude)
                             .second) {
                        throw std::invalid_argument{
                            field_context + " has duplicate named code value: " +
                            format_packed_integer(declarations_.packed_named_codes.value[code])};
                    }
                    if ((declarations_.packed_fields.kind[*field] !=
                             PackedFieldKind::unsigned_integer &&
                         declarations_.packed_fields.kind[*field] !=
                             PackedFieldKind::signed_integer) ||
                        std::string{types.cpp_types.spelling(field_type)} == "bool") {
                        throw std::invalid_argument{field_context +
                                                    " named codes require an integer field"};
                    }
                    auto const code_fits{
                        declarations_.packed_fields.kind[*field] == PackedFieldKind::signed_integer
                            ? packed_integer_fits_signed(
                                  declarations_.packed_named_codes.value[code], field_bits)
                            : packed_integer_fits_unsigned(
                                  declarations_.packed_named_codes.value[code], field_bits)};
                    if (!code_fits) {
                        throw std::invalid_argument{code_context + " does not fit the field width"};
                    }
                    if (declarations_.packed_named_codes.sentinel[code]) {
                        if (!declarations_.packed_fields.minimum_value[*field].has_value()) {
                            throw std::invalid_argument{code_context +
                                                        " sentinel requires a semantic range"};
                        }
                        if (packed_integer_less_equal(
                                *declarations_.packed_fields.minimum_value[*field],
                                declarations_.packed_named_codes.value[code]) &&
                            packed_integer_less_equal(
                                declarations_.packed_named_codes.value[code],
                                *declarations_.packed_fields.maximum_value[*field])) {
                            throw std::invalid_argument{
                                code_context + " sentinel must be outside the live semantic range"};
                        }
                    } else if (declarations_.packed_fields.minimum_value[*field].has_value() &&
                               (packed_integer_less(
                                    declarations_.packed_named_codes.value[code],
                                    *declarations_.packed_fields.minimum_value[*field]) ||
                                packed_integer_less(
                                    *declarations_.packed_fields.maximum_value[*field],
                                    declarations_.packed_named_codes.value[code]))) {
                        throw std::invalid_argument{
                            code_context + " non-sentinel must be inside the live semantic range"};
                    }
                }
            }

            std::vector<std::string> names{
                text(declarations_.packed_fields.name[*field]) + "_field",
            };
            if (quantized.has_value() || mini.has_value()) {
                names.push_back(text(declarations_.packed_fields.name[*field]) + "_encoded_type");
                names.push_back(text(declarations_.packed_fields.name[*field]) + "_encoded");
                names.push_back(text(declarations_.packed_fields.name[*field]) +
                                "_maximum_encoded");
                if (declarations_.packed_values.mutable_value[value]) {
                    names.push_back("set_" + text(declarations_.packed_fields.name[*field]) +
                                    "_encoded");
                }
            } else if (fixed.has_value()) {
                names.push_back(text(declarations_.packed_fields.name[*field]) + "_raw_type");
                names.push_back(text(declarations_.packed_fields.name[*field]) + "_raw");
                names.push_back(text(declarations_.packed_fields.name[*field]) + "_minimum_raw");
                names.push_back(text(declarations_.packed_fields.name[*field]) + "_maximum_raw");
                if (declarations_.packed_values.mutable_value[value]) {
                    names.push_back("set_" + text(declarations_.packed_fields.name[*field]) +
                                    "_raw");
                    names.push_back("set_" + text(declarations_.packed_fields.name[*field]) +
                                    "_value");
                }
            } else {
                names.push_back(text(declarations_.packed_fields.name[*field]) + "_type");
                names.push_back(text(declarations_.packed_fields.name[*field]));
                if (declarations_.packed_values.mutable_value[value]) {
                    names.push_back("set_" + text(declarations_.packed_fields.name[*field]));
                }
            }
            if (declarations_.packed_fields.kind[*field] == PackedFieldKind::enumeration) {
                names.push_back(text(declarations_.packed_fields.name[*field]) +
                                "_underlying_type");
            } else if (declarations_.packed_fields.kind[*field] ==
                           PackedFieldKind::signed_integer ||
                       (declarations_.packed_fields.kind[*field] ==
                            PackedFieldKind::unsigned_integer &&
                        std::string{types.cpp_types.spelling(field_type)} != "bool")) {
                names.push_back(text(declarations_.packed_fields.name[*field]) + "_range");
            }
            if (declarations_.packed_fields.range_helper[*field]) {
                names.push_back(text(declarations_.packed_fields.name[*field]) + "_range_fits");
            }
            auto const& generated_codes{
                scalar.has_value() ? integer_scalars.named_codes[integer_scalars.index(*scalar)]
                                   : declarations_.packed_fields.named_codes[*field]};
            for (auto const code_id : declarations_.items(generated_codes)) {
                auto const code{code_id};
                names.push_back(text(declarations_.packed_fields.name[*field]) + "_" +
                                text(declarations_.packed_named_codes.name[code]));
            }
            for (auto const& name : names) {
                if (!generated_names.insert(name).second) {
                    throw std::invalid_argument{field_context +
                                                " collides with generated API name: " + name};
                }
            }
        }
        if (!packed_default_value(value, manifest_, graph_, module_).has_value() &&
            generated_names.contains("RawTag")) {
            throw std::invalid_argument{context + " collides with generated API name: RawTag"};
        }
    }

    void validate_record(lispb::codegen::DeclarationHandle const& record,
                         TypeRegistry const& types) {
        require_identifier(text(declarations_.records.name[record]), "Record name");
        validate_export_specifier(declarations_.records.export_specifier[record],
                                  "Record '" + text(declarations_.records.name[record]) +
                                      "' export specifier");
        auto const row{declarations_.records.index(record)};
        auto const& records{declarations_.records};
        auto const& members{declarations_.record_members};
        if (records.generate_visitor[row]) {
            auto const record_name{text(records.name[row]) +
                                   (records.constraint_policy[row] ? "Value" : "")};
            if (record_name == "Field" || record_name == "visit_field" || record_name == "Self" ||
                record_name == "Visitor") {
                throw std::invalid_argument{"Record visitor conflicts with record name: " +
                                            record_name};
            }
            auto const check_name{[&](StringId name) {
                auto const value{text(name)};
                if (value == "Field" || value == "Count" || value == "visit_field") {
                    throw std::invalid_argument{"Record visitor reserves the name: " + value};
                }
            }};
            for (auto const id : declarations_.items(records.members[row])) {
                check_name(members.name[members.index(id)]);
            }
            for (auto const id : declarations_.items(records.functions[row])) {
                auto const& functions{declarations_.functions};
                check_name(functions.name[id]);
            }
        }
        if (records.constraint_policy[row]) {
            require_qualified_identifier(*records.constraint_policy[row],
                                         "Record constraint policy");
        }
        for (auto const id : declarations_.items(records.members[row])) {
            auto const member{members.index(id)};
            if (members.s7[member].name) {
                require_non_blank_value(*members.s7[member].name, "S7 member name");
            }
        }
        std::vector<std::string> member_names;
        member_names.reserve(records.members[row].size());
        for (auto const member_id : declarations_.items(declarations_.records.members[record])) {
            auto const member{member_id};
            auto const context{"Record '" + text(declarations_.records.name[record]) +
                               "' member '" + text(declarations_.record_members.name[member]) +
                               "'"};
            require_identifier(text(declarations_.record_members.name[member]),
                               "Record '" + text(declarations_.records.name[record]) +
                                   "' member name");
            validate_type(declarations_.record_members.type[member], types, context);
            if (declarations_.record_members.relationship[member].has_value()) {
                validate_semantic_relation(*declarations_.record_members.relationship[member],
                                           types,
                                           context + " relationship");
            }
            if (declarations_.record_members.count[member] == 0) {
                throw std::invalid_argument{context + " count must be greater than zero"};
            }
            member_names.push_back(text(declarations_.record_members.name[member]));
        }
        require_unique_names(member_names,
                             "Record '" + text(declarations_.records.name[record]) + "' members");
        if (declarations_.records.comparison_noexcept[record] &&
            declarations_.records.comparison[record] == RecordComparison::none) {
            throw std::invalid_argument{"Record comparison-noexcept requires a comparison"};
        }
        for (auto const member_id : declarations_.items(declarations_.records.members[record])) {
            auto const member{member_id};
            if (declarations_.record_members.initializer[member] &&
                view(*declarations_.record_members.initializer[member]).find_first_of(";\r\n#") !=
                    std::string::npos) {
                throw std::invalid_argument{
                    "Record initializer must be a brace initializer expression list"};
            }
        }
        std::set<std::string> signatures;
        for (auto const function_id :
             declarations_.items(declarations_.records.functions[record])) {
            auto const function{function_id};
            auto const context{"Record '" + text(declarations_.records.name[record]) +
                               "' function '" + text(declarations_.functions.name[function]) + "'"};
            if (text(declarations_.functions.name[function]) != "operator[]" &&
                text(declarations_.functions.name[function]) != "operator+" &&
                text(declarations_.functions.name[function]) != "operator-" &&
                text(declarations_.functions.name[function]) != "operator+=" &&
                text(declarations_.functions.name[function]) != "operator-=") {
                require_identifier(text(declarations_.functions.name[function]), context);
            }
            if (text(declarations_.functions.name[function]) ==
                    text(declarations_.records.name[record]) ||
                std::ranges::find(member_names, text(declarations_.functions.name[function])) !=
                    member_names.end()) {
                throw std::invalid_argument{context + " collides with a member or owning type"};
            }
            validate_function(function, types, context, signatures);
            if (declarations_.functions.definition_in_source[function] ||
                declarations_.functions.template_parameters[function] ||
                declarations_.functions.requires_clause[function]) {
                throw std::invalid_argument{
                    context + " requires an ordinary header method or bodyless declaration"};
            }
        }
    }

    void validate_union(lispb::codegen::DeclarationHandle const& schema,
                        TypeRegistry const& types) {
        require_identifier(text(declarations_.unions.name[schema]), "Union name");
        validate_export_specifier(declarations_.unions.export_specifier[schema],
                                  "Union '" + text(declarations_.unions.name[schema]) +
                                      "' export specifier");
        if (declarations_.unions.alternatives[schema].empty()) {
            throw std::invalid_argument{"Union '" + text(declarations_.unions.name[schema]) +
                                        "' must have alternatives"};
        }
        std::vector<std::string> alternative_names;
        alternative_names.reserve(declarations_.unions.alternatives[schema].size());
        for (auto const alternative_id :
             declarations_.items(declarations_.unions.alternatives[schema])) {
            auto const alternative{alternative_id};
            auto const context{"Union '" + text(declarations_.unions.name[schema]) +
                               "' alternative '" +
                               text(declarations_.union_alternatives.name[alternative]) + "'"};
            require_identifier(text(declarations_.union_alternatives.name[alternative]),
                               "Union '" + text(declarations_.unions.name[schema]) +
                                   "' alternative name");
            validate_type(declarations_.union_alternatives.type[alternative], types, context);
            if (declarations_.union_alternatives.count[alternative] == 0) {
                throw std::invalid_argument{context + " count must be greater than zero"};
            }
            alternative_names.push_back(text(declarations_.union_alternatives.name[alternative]));
        }
        require_unique_names(alternative_names,
                             "Union '" + text(declarations_.unions.name[schema]) +
                                 "' alternatives");
    }

    void validate_tagged_union(lispb::codegen::DeclarationHandle const& schema,
                               TypeRegistry const& types) {
        require_identifier(text(declarations_.tagged_unions.name[schema]), "Tagged union name");
        validate_export_specifier(declarations_.tagged_unions.export_specifier[schema],
                                  "Tagged union '" +
                                      text(declarations_.tagged_unions.name[schema]) +
                                      "' export specifier");
        validate_type(declarations_.tagged_unions.discriminant[schema],
                      types,
                      "Tagged union '" + text(declarations_.tagged_unions.name[schema]) +
                          "' discriminant");
        if (declarations_.tagged_unions.alternatives[schema].empty()) {
            throw std::invalid_argument{"Tagged union '" +
                                        text(declarations_.tagged_unions.name[schema]) +
                                        "' must have alternatives"};
        }
        std::vector<std::string> alternative_names;
        std::vector<std::string> tags;
        alternative_names.reserve(declarations_.tagged_unions.alternatives[schema].size());
        tags.reserve(declarations_.tagged_unions.alternatives[schema].size());
        for (auto const alternative_id :
             declarations_.items(declarations_.tagged_unions.alternatives[schema])) {
            auto const alternative{alternative_id};
            auto const context{"Tagged union '" + text(declarations_.tagged_unions.name[schema]) +
                               "' alternative '" +
                               text(declarations_.tagged_union_alternatives.name[alternative]) +
                               "'"};
            require_identifier(text(declarations_.tagged_union_alternatives.name[alternative]),
                               "Tagged union '" + text(declarations_.tagged_unions.name[schema]) +
                                   "' alternative name");
            require_identifier(text(declarations_.tagged_union_alternatives.tag[alternative]),
                               context + " tag");
            validate_type(
                declarations_.tagged_union_alternatives.type[alternative], types, context);
            if (declarations_.tagged_union_alternatives.count[alternative] == 0) {
                throw std::invalid_argument{context + " count must be greater than zero"};
            }
            alternative_names.push_back(
                text(declarations_.tagged_union_alternatives.name[alternative]));
            tags.push_back(text(declarations_.tagged_union_alternatives.tag[alternative]));
        }
        require_unique_names(alternative_names,
                             "Tagged union '" + text(declarations_.tagged_unions.name[schema]) +
                                 "' alternatives");
        require_unique_names(
            tags, "Tagged union '" + text(declarations_.tagged_unions.name[schema]) + "' tags");
    }

    void validate_integer_domain(bool const signedness,
                                 PackedIntegerValue const minimum_value,
                                 PackedIntegerValue const maximum_value,
                                 std::optional<std::uint32_t> const bit_width,
                                 SchemaRange<PackedNamedCodeId> const named_codes,
                                 std::string const& context) {
        if (packed_integer_less(maximum_value, minimum_value)) {
            throw std::invalid_argument{context + " minimum exceeds maximum"};
        }
        if (!signedness && minimum_value.negative) {
            throw std::invalid_argument{context + " unsigned domain contains a negative value"};
        }
        auto required_minimum{minimum_value};
        auto required_maximum{maximum_value};
        std::set<StringId> code_names;
        std::set<std::pair<bool, std::uint64_t>> code_values;
        for (auto const code_id : declarations_.items(named_codes)) {
            auto const code{code_id};
            auto const code_context{context + " code '" +
                                    text(declarations_.packed_named_codes.name[code]) + "'"};
            require_identifier(text(declarations_.packed_named_codes.name[code]),
                               code_context + " name");
            if (!code_names.insert(declarations_.packed_named_codes.name[code]).second) {
                throw std::invalid_argument{context + " has duplicate code: " +
                                            text(declarations_.packed_named_codes.name[code])};
            }
            if (!code_values
                     .emplace(declarations_.packed_named_codes.value[code].negative,
                              declarations_.packed_named_codes.value[code].magnitude)
                     .second) {
                throw std::invalid_argument{
                    context + " has duplicate code value: " +
                    format_packed_integer(declarations_.packed_named_codes.value[code])};
            }
            if (!signedness && declarations_.packed_named_codes.value[code].negative) {
                throw std::invalid_argument{code_context + " is negative in an unsigned domain"};
            }
            auto const inside_live_range{
                packed_integer_less_equal(minimum_value,
                                          declarations_.packed_named_codes.value[code]) &&
                packed_integer_less_equal(declarations_.packed_named_codes.value[code],
                                          maximum_value)};
            if (declarations_.packed_named_codes.sentinel[code] == inside_live_range) {
                throw std::invalid_argument{code_context +
                                            (declarations_.packed_named_codes.sentinel[code]
                                                 ? " sentinel must be outside the live range"
                                                 : " non-sentinel must be inside the live range")};
            }
            if (packed_integer_less(declarations_.packed_named_codes.value[code],
                                    required_minimum)) {
                required_minimum = declarations_.packed_named_codes.value[code];
            }
            if (packed_integer_less(required_maximum,
                                    declarations_.packed_named_codes.value[code])) {
                required_maximum = declarations_.packed_named_codes.value[code];
            }
        }

        auto const minimum_bits{
            minimum_packed_integer_bits(required_minimum, required_maximum, signedness)};
        if (!minimum_bits.has_value()) {
            throw std::invalid_argument{context +
                                        " domain does not fit the supported 1-64-bit range"};
        }
        if (bit_width.has_value() && (*bit_width == 0 || *bit_width > 64)) {
            throw std::invalid_argument{context + " bit width must be in the range 1..64"};
        }
        if (bit_width.has_value() && *bit_width < *minimum_bits) {
            throw std::invalid_argument{
                std::format("{} {}-bit width is smaller than the required {} bits",
                            context,
                            *bit_width,
                            *minimum_bits)};
        }
    }

    void validate_integer_scalar(lispb::codegen::DeclarationHandle const& scalar,
                                 TypeRegistry const& types) {
        auto const context{"Integer scalar '" + text(declarations_.integer_scalars.name[scalar]) +
                           "'"};
        require_identifier(text(declarations_.integer_scalars.name[scalar]), "Integer scalar name");

        if (declarations_.integer_scalars.relationship[scalar].has_value()) {
            validate_semantic_relation(*declarations_.integer_scalars.relationship[scalar],
                                       types,
                                       context + " relationship");
            auto const relation_kind{
                declarations_.semantic_relations
                    .kind[*declarations_.integer_scalars.relationship[scalar]]};
            auto const requires_unsigned{relation_kind == SemanticRelationKind::index_into ||
                                         relation_kind == SemanticRelationKind::count_of ||
                                         relation_kind == SemanticRelationKind::offset_into};
            if (requires_unsigned && declarations_.integer_scalars.signedness[scalar]) {
                throw std::invalid_argument{
                    std::format("{} relationship '{}' requires an unsigned integer scalar",
                                context,
                                to_serialized_string(relation_kind))};
            }
        }

        auto required_minimum{declarations_.integer_scalars.minimum_value[scalar]};
        auto required_maximum{declarations_.integer_scalars.maximum_value[scalar]};
        for (auto const code_id :
             declarations_.items(declarations_.integer_scalars.named_codes[scalar])) {
            auto const code{code_id};
            if (packed_integer_less(declarations_.packed_named_codes.value[code],
                                    required_minimum)) {
                required_minimum = declarations_.packed_named_codes.value[code];
            }
            if (packed_integer_less(required_maximum,
                                    declarations_.packed_named_codes.value[code])) {
                required_maximum = declarations_.packed_named_codes.value[code];
            }
        }

        if (declarations_.integer_scalars.cpp_emission[scalar] == IntegerScalarCppEmission::none) {
            if (declarations_.integer_scalars.cpp_type[scalar].has_value()) {
                throw std::invalid_argument{context + " C++ type requires an emission policy"};
            }
            return;
        }
        if (!declarations_.integer_scalars.cpp_type[scalar].has_value()) {
            throw std::invalid_argument{context + " C++ emission requires an explicit C++ type"};
        }
        if (declarations_.integer_scalars.cpp_emission[scalar] != IntegerScalarCppEmission::alias &&
            declarations_.integer_scalars.named_codes[scalar].empty()) {
            throw std::invalid_argument{context +
                                        " constants emission requires at least one named code"};
        }
        validate_type(*declarations_.integer_scalars.cpp_type[scalar],
                      types,
                      context + " C++ representation");
        auto const cpp_type{resolved_type(*declarations_.integer_scalars.cpp_type[scalar], types)};
        auto const storage_domain{
            enum_storage_domain(std::string{types.cpp_types.spelling(cpp_type)})};
        if (!storage_domain.has_value()) {
            throw std::invalid_argument{std::format("{} has unsupported C++ representation: {}",
                                                    context,
                                                    types.cpp_types.spelling(cpp_type))};
        }
        auto const fits = [&](PackedIntegerValue const value) {
            return storage_domain->is_unsigned
                     ? packed_integer_fits_unsigned(value, storage_domain->bit_width)
                     : packed_integer_fits_signed(value, storage_domain->bit_width);
        };
        if (!fits(required_minimum) || !fits(required_maximum)) {
            throw std::invalid_argument{
                std::format("{} domain does not fit C++ representation '{}'",
                            context,
                            types.cpp_types.spelling(cpp_type))};
        }
    }

    void validate_linear_quantized(lispb::codegen::DeclarationHandle const& representation,
                                   TypeRegistry const& types) {
        auto const& integer_scalars{graph_.definitions().integer_scalars};
        auto const context{"Linear quantization '" +
                           text(declarations_.linear_quantizeds.name[representation]) + "'"};
        require_identifier(text(declarations_.linear_quantizeds.name[representation]),
                           "Linear quantization name");

        validate_type(
            declarations_.linear_quantizeds.source[representation], types, context + " source");
        auto const source{find_integer_domain(
            declarations_.linear_quantizeds.source[representation], graph_, module_)};
        if (!source.has_value()) {
            throw std::invalid_argument{context + " source must have a declared integer domain"};
        }
        if (!packed_integer_less(integer_scalars.minimum_value[integer_scalars.index(*source)],
                                 integer_scalars.maximum_value[integer_scalars.index(*source)])) {
            throw std::invalid_argument{context +
                                        " source range must contain at least two distinct values"};
        }
        if (declarations_.linear_quantizeds.bit_width[representation] == 0 ||
            declarations_.linear_quantizeds.bit_width[representation] > 64) {
            throw std::invalid_argument{context + " bit width must be in the range 1..64"};
        }
        auto const maximum_reserved{
            declarations_.linear_quantizeds.bit_width[representation] == 64
                ? (std::numeric_limits<std::uint64_t>::max)() - 1
                : (std::uint64_t{1} << declarations_.linear_quantizeds.bit_width[representation]) -
                      2};
        if (declarations_.linear_quantizeds.reserved_codes[representation] > maximum_reserved) {
            throw std::invalid_argument{
                std::format("{} reserves {} codes but at least two usable codes are required",
                            context,
                            declarations_.linear_quantizeds.reserved_codes[representation])};
        }
    }

    void validate_integer_varint(lispb::codegen::DeclarationHandle const& representation,
                                 TypeRegistry const& types) {
        auto const& integer_scalars{graph_.definitions().integer_scalars};
        auto const context{"Integer varint '" +
                           text(declarations_.integer_varints.name[representation]) + "'"};
        require_identifier(text(declarations_.integer_varints.name[representation]),
                           "Integer varint name");

        validate_type(
            declarations_.integer_varints.source[representation], types, context + " source");
        auto const source{find_integer_domain(
            declarations_.integer_varints.source[representation], graph_, module_)};
        if (!source.has_value()) {
            throw std::invalid_argument{context + " source must have a declared integer domain"};
        }
        if (declarations_.integer_varints.encoding[representation] ==
                IntegerVarintEncoding::unsigned_varint &&
            integer_scalars.signedness[integer_scalars.index(*source)]) {
            throw std::invalid_argument{context + " unsigned encoding requires an unsigned source"};
        }
        if (declarations_.integer_varints.encoding[representation] !=
                IntegerVarintEncoding::unsigned_varint &&
            !integer_scalars.signedness[integer_scalars.index(*source)]) {
            throw std::invalid_argument{context +
                                        " signed and zigzag encodings require a signed source"};
        }
    }

    void validate_fixed_point(lispb::codegen::DeclarationHandle const& representation) {
        auto const context{"Fixed-point representation '" +
                           text(declarations_.fixed_points.name[representation]) + "'"};
        require_identifier(text(declarations_.fixed_points.name[representation]),
                           "Fixed-point representation name");

        if (declarations_.fixed_points.total_bits[representation] == 0 ||
            declarations_.fixed_points.total_bits[representation] > 64) {
            throw std::invalid_argument{context + " total width must be in the range 1..64"};
        }
        auto const maximum_fractional_bits{
            declarations_.fixed_points.signedness[representation]
                ? declarations_.fixed_points.total_bits[representation] - 1
                : declarations_.fixed_points.total_bits[representation]};
        if (declarations_.fixed_points.fractional_bits[representation] > maximum_fractional_bits) {
            throw std::invalid_argument{std::format(
                "{} fractional width must be at most {}{}",
                context,
                maximum_fractional_bits,
                declarations_.fixed_points.signedness[representation] ? " so one sign bit remains"
                                                                      : "")};
        }

        std::optional<PackedIntegerValue> minimum;
        std::optional<PackedIntegerValue> maximum;
        if (declarations_.fixed_points.minimum_value[representation].has_value()) {
            auto parsed{parse_fixed_point_value(
                view(*declarations_.fixed_points.minimum_value[representation]),
                declarations_.fixed_points.fractional_bits[representation])};
            if (!parsed.has_value()) {
                throw std::invalid_argument{context + " minimum: " + parsed.error()};
            }
            minimum = *parsed;
        }
        if (declarations_.fixed_points.maximum_value[representation].has_value()) {
            auto parsed{parse_fixed_point_value(
                view(*declarations_.fixed_points.maximum_value[representation]),
                declarations_.fixed_points.fractional_bits[representation])};
            if (!parsed.has_value()) {
                throw std::invalid_argument{context + " maximum: " + parsed.error()};
            }
            maximum = *parsed;
        }
        auto const fits = [&](PackedIntegerValue const value) {
            return declarations_.fixed_points.signedness[representation]
                     ? packed_integer_fits_signed(
                           value, declarations_.fixed_points.total_bits[representation])
                     : packed_integer_fits_unsigned(
                           value, declarations_.fixed_points.total_bits[representation]);
        };
        if ((minimum.has_value() && !fits(*minimum)) || (maximum.has_value() && !fits(*maximum))) {
            throw std::invalid_argument{context + " bounds must fit the representation width"};
        }
        if (minimum.has_value() && maximum.has_value() && packed_integer_less(*maximum, *minimum)) {
            throw std::invalid_argument{context + " maximum must be at least minimum"};
        }
    }

    void validate_mini_float(lispb::codegen::DeclarationHandle const& representation) {
        auto const context{"Mini-float representation '" +
                           text(declarations_.mini_floats.name[representation]) + "'"};
        require_identifier(text(declarations_.mini_floats.name[representation]),
                           "Mini-float representation name");

        if (declarations_.mini_floats.sign_bits[representation] > 1) {
            throw std::invalid_argument{context + " sign width must be zero or one"};
        }
        if (declarations_.mini_floats.exponent_bits[representation] < 2 ||
            declarations_.mini_floats.exponent_bits[representation] > 15) {
            throw std::invalid_argument{context + " exponent width must be in the range 2..15"};
        }
        if (declarations_.mini_floats.significand_bits[representation] > 62) {
            throw std::invalid_argument{context + " significand width must be in the range 0..62"};
        }
        auto const total_bits{declarations_.mini_floats.sign_bits[representation] +
                              declarations_.mini_floats.exponent_bits[representation] +
                              declarations_.mini_floats.significand_bits[representation]};
        if (total_bits > 64) {
            throw std::invalid_argument{context + " total width must not exceed 64 bits"};
        }
        if (declarations_.mini_floats.exponent_bias[representation] < -32'768 ||
            declarations_.mini_floats.exponent_bias[representation] > 32'767) {
            throw std::invalid_argument{context +
                                        " exponent bias must be in the range -32768..32767"};
        }
    }

    void validate_optional_sentinel(lispb::codegen::DeclarationHandle const& representation,
                                    TypeRegistry const& types) {
        auto const& integer_scalars{graph_.definitions().integer_scalars};
        auto const context{"Optional sentinel representation '" +
                           text(declarations_.optional_sentinels.name[representation]) + "'"};
        require_identifier(text(declarations_.optional_sentinels.name[representation]),
                           "Optional sentinel representation name");

        validate_type(
            declarations_.optional_sentinels.source[representation], types, context + " source");
        auto const source{find_integer_domain(
            declarations_.optional_sentinels.source[representation], graph_, module_)};
        if (!source.has_value()) {
            throw std::invalid_argument{context + " source must have a declared integer domain"};
        }
        require_identifier(text(declarations_.optional_sentinels.sentinel[representation]),
                           context + " sentinel code");
        auto const code{std::ranges::find(
            declarations_.items(integer_scalars.named_codes[integer_scalars.index(*source)]),
            declarations_.optional_sentinels.sentinel[representation],
            [&](auto id) { return declarations_.packed_named_codes.name[id]; })};
        if (code == declarations_.items(integer_scalars.named_codes[integer_scalars.index(*source)])
                        .end()) {
            auto const has_sentinel{std::ranges::any_of(
                declarations_.items(integer_scalars.named_codes[integer_scalars.index(*source)]),
                [&](auto id) { return declarations_.packed_named_codes.sentinel[id]; })};
            throw std::invalid_argument{
                has_sentinel
                    ? context + " names unknown source code '" +
                          text(declarations_.optional_sentinels.sentinel[representation]) + "'"
                    : context + " source has no named sentinel codes"};
        }
        if (!declarations_.packed_named_codes.sentinel[*code]) {
            throw std::invalid_argument{
                context + " code '" +
                text(declarations_.optional_sentinels.sentinel[representation]) +
                "' is not a sentinel"};
        }
    }

    void validate_optional_presence_bit(lispb::codegen::DeclarationHandle const& representation,
                                        TypeRegistry const& types) {
        auto const context{"Optional presence-bit representation '" +
                           text(declarations_.optional_presence_bits.name[representation]) + "'"};
        require_identifier(text(declarations_.optional_presence_bits.name[representation]),
                           "Optional presence-bit representation name");

        validate_type(declarations_.optional_presence_bits.source[representation],
                      types,
                      context + " source");
        if (!find_integer_domain(
                 declarations_.optional_presence_bits.source[representation], graph_, module_)
                 .has_value()) {
            throw std::invalid_argument{context + " source must have a declared integer domain"};
        }
    }

    void validate_soa(lispb::codegen::ModuleHandle const& module,
                      std::span<lispb::codegen::DeclarationHandle const> schemas,
                      TypeRegistry const& types) {
        for (auto const& schema : schemas) {
            require_identifier(text(declarations_.soas.name[schema]), "SOA name");
            auto const context{"SOA '" + text(declarations_.soas.name[schema]) + "'"};
            require_identifier(soa_namespace_name(text(declarations_.soas.name[schema])),
                               context + " namespace");
            if (declarations_.soas.layout_only[schema] &&
                !declarations_.soas.functions[schema].empty()) {
                throw std::invalid_argument{context +
                                            " layout-only has no owner; use view functions"};
            }

            if (declarations_.soas.field_mask_name[schema].has_value() !=
                declarations_.soas.field_enum_name[schema].has_value()) {
                throw std::invalid_argument{
                    "SOA '" + text(declarations_.soas.name[schema]) +
                    "' must specify both field-mask-name and field-enum-name"};
            }
            if (declarations_.soas.field_mask_name[schema].has_value()) {
                require_identifier(*declarations_.soas.field_mask_name[schema],
                                   "SOA '" + text(declarations_.soas.name[schema]) +
                                       "' field mask");
                require_identifier(*declarations_.soas.field_enum_name[schema],
                                   "SOA '" + text(declarations_.soas.name[schema]) +
                                       "' field enum");
            }
            if (declarations_.soas.members[schema].empty()) {
                throw std::invalid_argument{"SOA '" + text(declarations_.soas.name[schema]) +
                                            "' must have members"};
            }
            std::vector<std::string> member_names;
            if (declarations_.soas.equivalent_constructor[schema] &&
                !declarations_.soas.equivalent_type[schema]) {
                throw std::invalid_argument{
                    "SOA equivalent constructor requires an equivalent type"};
            }
            auto mapped_members{std::size_t{}};
            int mask_storage_member_count{};
            int mask_field_count{};
            for (auto const member_id : declarations_.items(declarations_.soas.members[schema])) {
                auto const member{member_id};
                auto const member_row{declarations_.soa_members.index(member)};
                if (declarations_.soa_members.equivalent_member[member_row]) {
                    ++mapped_members;
                    if (!declarations_.soas.equivalent_type[schema]) {
                        throw std::invalid_argument{
                            "SOA equivalent member mapping requires an equivalent type"};
                    }
                } else if (declarations_.soa_members.equivalent_accessor[member_row]) {
                    throw std::invalid_argument{
                        "SOA equivalent accessor requires a member mapping"};
                }
                auto const context{"SOA '" + text(declarations_.soas.name[schema]) + "' member '" +
                                   text(declarations_.soa_members.name[member]) + "'"};
                member_names.push_back(text(declarations_.soa_members.name[member]));
                require_identifier(text(declarations_.soa_members.name[member]),
                                   "SOA '" + text(declarations_.soas.name[schema]) +
                                       "' member name");
                reject_generated_name_collision(text(declarations_.soa_members.name[member]),
                                                "SOA '" + text(declarations_.soas.name[schema]) +
                                                    "' member name",
                                                true);
                validate_type(declarations_.soa_members.type[member], types, context);
                if (declarations_.soa_members.relationship[member].has_value()) {
                    validate_semantic_relation(*declarations_.soa_members.relationship[member],
                                               types,
                                               context + " relationship");
                }
                if (declarations_.soas.field_mask_name[schema].has_value() &&
                    declarations_.type_refs.name[declarations_.soa_members.type[member]] ==
                        *declarations_.soas.field_mask_name[schema]) {
                    ++mask_storage_member_count;
                    if (declarations_.soa_members.kind[member] != SoaMemberKind::array) {
                        throw std::invalid_argument{"SOA '" +
                                                    text(declarations_.soas.name[schema]) +
                                                    "' field-mask storage must be an array member"};
                    }
                }
                if (!declarations_.soa_members.mask_dimensions[member].empty() &&
                    !declarations_.soa_members.mask_field[member]) {
                    throw std::invalid_argument{"SOA '" + text(declarations_.soas.name[schema]) +
                                                "' member '" +
                                                text(declarations_.soa_members.name[member]) +
                                                "' has mask dimensions but is not a mask field"};
                }
                if (declarations_.soa_members.mask_field[member]) {
                    ++mask_field_count;
                    if (!declarations_.soas.field_mask_name[schema].has_value()) {
                        throw std::invalid_argument{
                            "SOA '" + text(declarations_.soas.name[schema]) + "' member '" +
                            text(declarations_.soa_members.name[member]) +
                            "' is a mask field without a generated field mask"};
                    }
                    if (declarations_.soa_members.kind[member] != SoaMemberKind::array) {
                        throw std::invalid_argument{
                            "SOA '" + text(declarations_.soas.name[schema]) + "' mask field '" +
                            text(declarations_.soa_members.name[member]) +
                            "' must be an array member"};
                    }
                    std::set<std::string> dimension_names;
                    for (auto const dimension_id :
                         declarations_.items(declarations_.soa_members.mask_dimensions[member])) {
                        auto const dimension{dimension_id};
                        require_identifier(
                            text(declarations_.soa_mask_dimensions.index_name[dimension]),
                            "SOA '" + text(declarations_.soas.name[schema]) +
                                "' mask dimension index");
                        require_value(text(declarations_.soa_mask_dimensions.extent[dimension]),
                                      "SOA '" + text(declarations_.soas.name[schema]) +
                                          "' mask dimension extent");
                        if (!dimension_names
                                 .insert(
                                     text(declarations_.soa_mask_dimensions.index_name[dimension]))
                                 .second) {
                            throw std::invalid_argument{
                                "SOA '" + text(declarations_.soas.name[schema]) + "' mask field '" +
                                text(declarations_.soa_members.name[member]) +
                                "' has duplicate dimension names"};
                        }
                    }
                }
            }
            if (declarations_.soas.field_mask_name[schema].has_value() &&
                mask_storage_member_count != 1) {
                throw std::invalid_argument{
                    "SOA '" + text(declarations_.soas.name[schema]) +
                    "' must have exactly one array member of its field-mask type"};
            }
            if (declarations_.soas.field_mask_name[schema].has_value() && mask_field_count == 0) {
                throw std::invalid_argument{
                    "SOA '" + text(declarations_.soas.name[schema]) +
                    "' generated field mask must contain at least one field"};
            }
            if (mapped_members != 0 && mapped_members != member_names.size()) {
                throw std::invalid_argument{
                    "SOA equivalent member mappings must cover every column"};
            }
            require_unique_names(member_names,
                                 "SOA '" + text(declarations_.soas.name[schema]) + "' members");
            validate_export_specifier(declarations_.soas.export_specifier[schema],
                                      "SOA '" + text(declarations_.soas.name[schema]) +
                                          "' export specifier");
            for (auto const declaration :
                 declarations_.items(declarations_.soas.using_declarations[schema])) {
                require_non_blank_value(declaration,
                                        "SOA '" + text(declarations_.soas.name[schema]) +
                                            "' using declaration");
            }
            if (declarations_.soas.equivalent_type[schema].has_value()) {
                validate_type(*declarations_.soas.equivalent_type[schema],
                              types,
                              "SOA '" + text(declarations_.soas.name[schema]) + "' equivalent");
            }
            auto validate_functions = [&](SchemaRange<FunctionSchemaId> functions,
                                          std::set<std::string>& function_signatures,
                                          bool const read_only,
                                          bool const owner = false) {
                for (auto const function_id : declarations_.items(functions)) {
                    auto const function{function_id};
                    auto const context{"SOA '" + text(declarations_.soas.name[schema]) +
                                       "' function '" +
                                       text(declarations_.functions.name[function]) + "'"};
                    validate_function(function, types, context, function_signatures, owner);
                    if (auto const columns{declarations_.functions.append_columns[function]}) {
                        auto const& members{declarations_.soa_members};
                        auto const declared{
                            declarations_.items(declarations_.soas.members[schema])};
                        for (auto const column : declarations_.items(*columns)) {
                            auto const found{std::ranges::find(declared, column, [&](auto member) {
                                return members.name[members.index(member)];
                            })};
                            if (found == declared.end() ||
                                members.kind[members.index(*found)] != SoaMemberKind::array) {
                                throw std::invalid_argument{
                                    context + " append column must name a direct array member: " +
                                    text(column)};
                            }
                        }
                    }
                    if (read_only && !declarations_.functions.is_const[function] &&
                        !declarations_.functions.is_static[function]) {
                        throw std::invalid_argument{context +
                                                    " read-only view functions must be const"};
                    }
                    if (declarations_.functions.definition_in_source[function] &&
                        !declarations_.module_settings_store
                             .source[declarations_.normal_modules.settings[module]]) {
                        throw std::invalid_argument{
                            context + " out-of-line function requires a source output"};
                    }
                    if (declarations_.functions.is_static[function] &&
                        std::ranges::any_of(
                            declarations_.items(declarations_.functions.body_lines[function]),
                            [&](auto const line) {
                                return view(line).find("$column(") != std::string::npos;
                            })) {
                        throw std::invalid_argument{
                            context + " static function cannot access a receiver column"};
                    }
                    for (auto const body :
                         declarations_.items(declarations_.functions.body_lines[function])) {
                        validate_soa_body_columns(graph_, module, schema, view(body));
                    }
                    require_identifier(text(declarations_.functions.name[function]),
                                       "SOA '" + text(declarations_.soas.name[schema]) +
                                           "' function name");
                    if (text(declarations_.functions.name[function]) ==
                        text(declarations_.soas.name[schema])) {
                        throw std::invalid_argument{context + " collides with its owning type"};
                    }
                    reject_generated_name_collision(
                        text(declarations_.functions.name[function]), context, false);
                }
            };
            std::set<std::string> owner_signatures;
            std::set<std::string> view_signatures;
            validate_functions(declarations_.soas.functions[schema], owner_signatures, false, true);
            validate_functions(
                declarations_.soas.const_view_functions[schema], view_signatures, true);
            validate_functions(
                declarations_.soas.mutable_view_functions[schema], view_signatures, false);
        }
        for (auto const& root : schemas) {
            std::set<std::string> flattened_names;
            auto visit = [&](auto&& self,
                             lispb::codegen::DeclarationHandle schema,
                             lispb::codegen::ModuleHandle current_module,
                             std::string const& prefix) -> void {
                std::set<std::string> accessors;
                for (auto const member_id :
                     declarations_.items(declarations_.soas.members[schema])) {
                    auto const member{member_id};
                    auto const flattened{prefix.empty()
                                             ? text(declarations_.soa_members.name[member])
                                             : prefix + "_" +
                                                   text(declarations_.soa_members.name[member])};
                    if (flattened == "column_count" || flattened.contains("__") ||
                        flattened.back() == '_') {
                        throw std::invalid_argument{
                            "Single-allocation leaf would generate reserved identifiers: " +
                            flattened};
                    }
                    auto const accessor{declarations_.soa_members.kind[member] ==
                                                SoaMemberKind::nested
                                            ? "view_" + text(declarations_.soa_members.name[member])
                                            : text(declarations_.soa_members.name[member])};
                    if (!accessors.insert(accessor).second) {
                        throw std::invalid_argument{"Duplicate compact view accessor: " + accessor};
                    }
                    if (text(declarations_.soa_members.name[member]) == "validate" ||
                        text(declarations_.soa_members.name[member]) == "capacity_blocks") {
                        throw std::invalid_argument{"Member collides with compact view API: " +
                                                    text(declarations_.soa_members.name[member])};
                    }
                    if (declarations_.soa_members.kind[member] != SoaMemberKind::nested) {
                        if (!flattened_names.insert(flattened).second) {
                            throw std::invalid_argument{
                                "Single-allocation flattened leaf name collision: " + flattened};
                        }
                        continue;
                    }
                    auto const occurrence{
                        graph_.resolved(current_module, declarations_.soa_members.type[member])};
                    auto const& occurrences{graph_.occurrences()};
                    auto const child{occurrences.type[occurrences.index(occurrence)]};
                    self(self, graph_.source_declaration(child), graph_.module(child), flattened);
                }
            };
            visit(visit, root, module, {});
        }
    }

    void validate_static_table(lispb::codegen::DeclarationHandle const& table,
                               TypeRegistry const& types) {
        require_identifier(text(declarations_.static_tables.name[table]), "Static table name");

        validate_export_specifier(declarations_.static_tables.export_specifier[table],
                                  "Static table '" + text(declarations_.static_tables.name[table]) +
                                      "' export specifier");
        if (declarations_.static_tables.rows[table].empty()) {
            throw std::invalid_argument{"Static table '" +
                                        text(declarations_.static_tables.name[table]) +
                                        "' must have rows"};
        }
        if (declarations_.static_tables.columns[table].empty()) {
            throw std::invalid_argument{"Static table '" +
                                        text(declarations_.static_tables.name[table]) +
                                        "' must have columns"};
        }

        std::vector<std::string> row_names;
        std::set<std::string> generated_names{text(declarations_.static_tables.name[table]),
                                              "num_rows",
                                              "num",
                                              "apply_arrays",
                                              "apply_array_pairs"};
        for (auto const row_id : declarations_.items(declarations_.static_tables.rows[table])) {
            auto const row{row_id};
            require_identifier(text(declarations_.static_table_rows.name[row]),
                               "Static table '" + text(declarations_.static_tables.name[table]) +
                                   "' row name");
            row_names.push_back(text(declarations_.static_table_rows.name[row]));
            generated_names.insert(text(declarations_.static_table_rows.name[row]) + "_index");
        }
        require_unique_names(
            row_names, "Static table '" + text(declarations_.static_tables.name[table]) + "' rows");

        std::vector<std::string> column_names;
        for (auto const column_id :
             declarations_.items(declarations_.static_tables.columns[table])) {
            auto const column{column_id};
            require_identifier(text(declarations_.static_table_columns.name[column]),
                               "Static table '" + text(declarations_.static_tables.name[table]) +
                                   "' column name");
            if (generated_names.contains(text(declarations_.static_table_columns.name[column]))) {
                throw std::invalid_argument{
                    "Static table '" + text(declarations_.static_tables.name[table]) +
                    "' column '" + text(declarations_.static_table_columns.name[column]) +
                    "' collides with generated API"};
            }
            column_names.push_back(text(declarations_.static_table_columns.name[column]));
            validate_type(declarations_.static_table_columns.type[column],
                          types,
                          "Static table '" + text(declarations_.static_tables.name[table]) +
                              "' column '" + text(declarations_.static_table_columns.name[column]) +
                              "'");
        }
        require_unique_names(column_names,
                             "Static table '" + text(declarations_.static_tables.name[table]) +
                                 "' columns");

        std::set<std::string> column_name_set{column_names.begin(), column_names.end()};
        std::set<std::string> member_names{generated_names};
        member_names.insert(column_names.begin(), column_names.end());
        std::set<std::string> group_names;
        for (auto const group_id : declarations_.items(declarations_.static_tables.groups[table])) {
            auto const group{group_id};
            auto const context{"Static table '" + text(declarations_.static_tables.name[table]) +
                               "' group '" + text(declarations_.static_table_groups.name[group]) +
                               "'"};
            require_identifier(text(declarations_.static_table_groups.name[group]),
                               "Static table '" + text(declarations_.static_tables.name[table]) +
                                   "' group name");
            if (!group_names.insert(text(declarations_.static_table_groups.name[group])).second) {
                throw std::invalid_argument{"Duplicate static table group name: " +
                                            text(declarations_.static_table_groups.name[group])};
            }
            validate_type(declarations_.static_table_groups.type[group], types, context + " type");
            if (declarations_.static_table_groups.columns[group].empty()) {
                throw std::invalid_argument{context + " must have columns"};
            }

            for (auto const column_name :
                 declarations_.items(declarations_.static_table_groups.columns[group])) {
                require_identifier(column_name, context + " column name");
                if (!column_name_set.contains(text(column_name))) {
                    throw std::invalid_argument{context + " references unknown column '" +
                                                text(column_name) + "'"};
                }
            }
            require_unique_names(declarations_.static_table_groups.columns[group],
                                 context + " columns");

            auto const getter_name{"get_" + text(declarations_.static_table_groups.name[group])};
            if (!member_names.insert(getter_name).second) {
                throw std::invalid_argument{context + " getter '" + getter_name +
                                            "' collides with another table member"};
            }
        }
    }

    void validate_facade(lispb::codegen::DeclarationHandle const& facade,
                         lispb::codegen::ModuleSettingsId const& settings,
                         TypeRegistry const& types) {
        require_identifier(text(declarations_.facades.name[facade]), "Facade name");
        require_identifier(text(declarations_.facades.target_member_name[facade]),
                           "Facade '" + text(declarations_.facades.name[facade]) +
                               "' target member");
        validate_type(declarations_.facades.target_type[facade],
                      types,
                      "Facade '" + text(declarations_.facades.name[facade]) + "' target");
        if (declarations_.facades.methods[facade].empty()) {
            throw std::invalid_argument{"Facade '" + text(declarations_.facades.name[facade]) +
                                        "' must have methods"};
        }
        if (declarations_.facades.definitions_in_source[facade] &&
            !declarations_.module_settings_store.source[settings].has_value()) {
            throw std::invalid_argument{"Facade '" + text(declarations_.facades.name[facade]) +
                                        "' definitions in source require a source output"};
        }

        require_unique_names(declarations_.facades.friends[facade],
                             "Facade '" + text(declarations_.facades.name[facade]) + "' friends");
        for (auto const friend_name : declarations_.items(declarations_.facades.friends[facade])) {
            auto name{view(friend_name)};
            if (name.starts_with("::")) {
                name.remove_prefix(2);
            }
            require_qualified_identifier(
                name, "Facade '" + text(declarations_.facades.name[facade]) + "' friend");
        }
        validate_export_specifier(declarations_.facades.export_specifier[facade],
                                  "Facade '" + text(declarations_.facades.name[facade]) +
                                      "' export specifier");
        if (declarations_.facades.reference_target[facade] &&
            !declarations_.facades.validation_lines[facade].empty()) {
            throw std::invalid_argument{"Facade '" + text(declarations_.facades.name[facade]) +
                                        "' reference targets cannot have target validation"};
        }
        if (text(declarations_.facades.target_member_name[facade]) == "bind") {
            throw std::invalid_argument{"Facade '" + text(declarations_.facades.name[facade]) +
                                        "' target member collides with bind"};
        }
        for (auto const dependency :
             declarations_.items(declarations_.facades.validation_dependencies[facade])) {
            validate_dependency(
                dependency, types, "Facade '" + text(declarations_.facades.name[facade]) + "'");
        }
        std::set<std::string> method_signatures;
        if (!declarations_.facades.reference_target[facade]) {
            method_signatures.insert(
                signature(strings_.intern("bind"),
                          {std::string{types.cpp_types.spelling(
                               resolved_type(declarations_.facades.target_type[facade], types))} +
                           "&"},
                          false));
        }
        for (auto const method_id : declarations_.items(declarations_.facades.methods[facade])) {
            auto const method{method_id};
            require_identifier(text(declarations_.facade_methods.name[method]),
                               "Facade '" + text(declarations_.facades.name[facade]) +
                                   "' method name");
            if (text(declarations_.facade_methods.name[method]) ==
                    text(declarations_.facades.target_member_name[facade]) ||
                text(declarations_.facade_methods.name[method]) ==
                    text(declarations_.facades.name[facade])) {
                throw std::invalid_argument{"Facade '" + text(declarations_.facades.name[facade]) +
                                            "' method '" +
                                            text(declarations_.facade_methods.name[method]) +
                                            "' collides with a generated member"};
            }
            if (declarations_.facade_methods.target_name[method].has_value()) {
                require_identifier(*declarations_.facade_methods.target_name[method],
                                   "Facade '" + text(declarations_.facades.name[facade]) +
                                       "' method target name");
            }
            validate_type(declarations_.facade_methods.return_type[method],
                          types,
                          "Facade '" + text(declarations_.facades.name[facade]) + "' method '" +
                              text(declarations_.facade_methods.name[method]) + "' return");
            auto const context{"Facade '" + text(declarations_.facades.name[facade]) +
                               "' method '" + text(declarations_.facade_methods.name[method]) +
                               "'"};
            auto const parameter_types{parameter_type_names(
                declarations_.facade_methods.parameters[method], types, context)};
            auto const method_signature{signature(declarations_.facade_methods.name[method],
                                                  parameter_types,
                                                  declarations_.facade_methods.is_const[method])};
            if (!method_signatures.insert(method_signature).second) {
                throw std::invalid_argument{"Duplicate " + context + " signature"};
            }
        }
    }

    void validate_settings_module(lispb::codegen::ModuleHandle const& module,
                                  TypeRegistry const& types) {
        auto const context{"Settings module '" +
                           text(declarations_.module_settings_store
                                    .name[declarations_.settings_modules.settings[module]]) +
                           "'"};
        if (!declarations_.module_settings_store
                 .source[declarations_.settings_modules.settings[module]]
                 .has_value()) {
            throw std::invalid_argument{context + " must have a source output"};
        }
        require_identifier(text(declarations_.settings_modules.api_name[module]),
                           context + " API name");
        require_identifier(text(declarations_.settings_modules.state_name[module]),
                           context + " state name");
        validate_export_specifier(declarations_.settings_modules.export_specifier[module],
                                  context + " export specifier");
        if (declarations_.settings_modules.categories[module].empty()) {
            throw std::invalid_argument{context + " must have categories"};
        }
        if (declarations_.settings_modules.settings_list[module].empty()) {
            throw std::invalid_argument{context + " must have settings"};
        }

        std::set<std::string> category_names;
        for (auto const category_id :
             declarations_.items(declarations_.settings_modules.categories[module])) {
            auto const category{category_id};
            require_identifier(text(declarations_.setting_categories.name[category]),
                               context + " category name");
            require_value(text(declarations_.setting_categories.label[category]),
                          context + " category label");
            if (!category_names.insert(text(declarations_.setting_categories.name[category]))
                     .second) {
                throw std::invalid_argument{context + " has duplicate category: " +
                                            text(declarations_.setting_categories.name[category])};
            }
        }

        std::set<std::string> setting_names;
        for (auto const setting_id :
             declarations_.items(declarations_.settings_modules.settings_list[module])) {
            auto const setting{setting_id};
            auto const control{declarations_.settings.control[setting]};
            auto const setting_context{context + " setting '" +
                                       text(declarations_.settings.name[setting]) + "'"};
            require_identifier(text(declarations_.settings.name[setting]),
                               setting_context + " name");
            if (!setting_names.insert(text(declarations_.settings.name[setting])).second) {
                throw std::invalid_argument{context + " has duplicate setting: " +
                                            text(declarations_.settings.name[setting])};
            }
            require_value(text(declarations_.settings.label[setting]), setting_context + " label");
            if (declarations_.settings.tooltip[setting].has_value()) {
                require_value(*declarations_.settings.tooltip[setting],
                              setting_context + " tooltip");
            }
            if (!category_names.contains(text(declarations_.settings.category[setting]))) {
                throw std::invalid_argument{setting_context + " has unknown category: " +
                                            text(declarations_.settings.category[setting])};
            }
            validate_type(
                declarations_.settings.value_type[setting], types, setting_context + " value");
            require_identifier(text(declarations_.settings.backend[setting]),
                               setting_context + " backend");
            if (declarations_.setting_controls.options_provider[control].has_value()) {
                require_identifier(*declarations_.setting_controls.options_provider[control],
                                   setting_context + " options provider");
            }
            if (declarations_.setting_controls.availability_provider[control].has_value()) {
                require_identifier(*declarations_.setting_controls.availability_provider[control],
                                   setting_context + " availability provider");
            }

            auto const has_range{declarations_.setting_controls.minimum[control].has_value() ||
                                 declarations_.setting_controls.maximum[control].has_value() ||
                                 declarations_.setting_controls.step[control].has_value()};
            auto const is_range{
                declarations_.setting_controls.kind[control] == SettingControlKind::float_range ||
                declarations_.setting_controls.kind[control] == SettingControlKind::integer_range};
            if (is_range) {
                if (!declarations_.setting_controls.minimum[control].has_value() ||
                    !declarations_.setting_controls.maximum[control].has_value() ||
                    !declarations_.setting_controls.step[control].has_value()) {
                    throw std::invalid_argument{setting_context +
                                                " range requires min, max, and step"};
                }
                if (*declarations_.setting_controls.minimum[control] >=
                        *declarations_.setting_controls.maximum[control] ||
                    *declarations_.setting_controls.step[control] <= 0.0) {
                    throw std::invalid_argument{setting_context + " has an invalid range"};
                }
            } else if (has_range) {
                throw std::invalid_argument{setting_context +
                                            " non-range control has range values"};
            }
            if (declarations_.setting_controls.kind[control] == SettingControlKind::choice &&
                !declarations_.setting_controls.options_provider[control].has_value()) {
                throw std::invalid_argument{setting_context +
                                            " choice requires an options provider"};
            }
            if (declarations_.setting_controls.kind[control] == SettingControlKind::custom) {
                if (!declarations_.setting_controls.custom_row[control].has_value()) {
                    throw std::invalid_argument{setting_context +
                                                " custom control requires custom_row"};
                }
                require_identifier(*declarations_.setting_controls.custom_row[control],
                                   setting_context + " custom row");
            } else if (declarations_.setting_controls.custom_row[control].has_value()) {
                throw std::invalid_argument{setting_context + " non-custom control has custom_row"};
            }
            auto const type{std::string{types.cpp_types.spelling(
                resolved_type(declarations_.settings.value_type[setting], types))}};
            if (declarations_.setting_controls.kind[control] == SettingControlKind::toggle &&
                type != "bool") {
                throw std::invalid_argument{setting_context + " toggle value type must be bool"};
            }
            if (declarations_.setting_controls.kind[control] == SettingControlKind::float_range &&
                type != "float") {
                throw std::invalid_argument{setting_context +
                                            " float range value type must be float"};
            }
            if (declarations_.setting_controls.kind[control] == SettingControlKind::integer_range &&
                type != "std::int32_t") {
                throw std::invalid_argument{setting_context +
                                            " integer range value type must be std::int32_t"};
            }
        }
    }

    void validate_normal_module(lispb::codegen::ModuleHandle const& module,
                                Manifest const& manifest,
                                std::map<std::string, DeclarationHandle>& generated_names) {
        if (declarations_.normal_modules.enum_helper_namespace[module].has_value()) {
            require_qualified_identifier(
                *declarations_.normal_modules.enum_helper_namespace[module],
                "Module helper namespace");
        }
        std::vector<lispb::codegen::DeclarationHandle> schemas;
        std::set<lispb::StringId> names;
        std::set<lispb::StringId> local_generated_names;
        auto const& modules{declarations_.normal_modules};
        auto const& settings{declarations_.module_settings_store};
        auto const settings_row{settings.index(modules.settings[modules.index(module)])};
        auto const namespace_name{settings.namespace_name[settings_row]};
        auto const prefix{namespace_name ? text(*namespace_name) + "::" : std::string{}};
        for (auto const declaration :
             declarations_.items(declarations_.normal_modules.declarations[module])) {
            auto const name{declaration_name(declarations_, declaration)};
            if (!names.insert(name).second) {
                throw std::invalid_argument{
                    "Duplicate declaration name in module '" +
                    text(declarations_.module_settings_store
                             .name[declarations_.normal_modules.settings[module]]) +
                    "': " + text(name)};
            }
            for (auto const generated_name : generated_cpp_names(declarations_, declaration)) {
                auto const [existing, inserted]{
                    generated_names.emplace(prefix + text(generated_name), declaration)};
                // Preserve ordinary cross-module ambiguity handling while reserving SoA scopes.
                auto const soa_collision{!inserted &&
                                         (declaration.kind() == DeclarationKind::soa ||
                                          existing->second.kind() == DeclarationKind::soa)};
                if (!local_generated_names.insert(generated_name).second || soa_collision) {
                    throw std::invalid_argument{
                        "Generated C++ name collision in module '" +
                        text(declarations_.module_settings_store
                                 .name[declarations_.normal_modules.settings[module]]) +
                        "': " + text(generated_name)};
                }
            }
            [&]() {
                auto const value{declaration};
                switch (value.kind()) {
                    case lispb::codegen::DeclarationKind::enumeration: {

                        validate_enum(value, module, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::packed_value: {

                        validate_packed_value(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::record: {

                        validate_record(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::union_type: {

                        validate_union(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::tagged_union: {

                        validate_tagged_union(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::integer_scalar: {

                        validate_integer_scalar(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::linear_quantized: {

                        validate_linear_quantized(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::integer_varint: {

                        validate_integer_varint(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::fixed_point: {

                        validate_fixed_point(value);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::mini_float: {

                        validate_mini_float(value);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::optional_sentinel: {

                        validate_optional_sentinel(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::optional_presence_bit: {

                        validate_optional_presence_bit(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::static_table: {

                        validate_static_table(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::facade: {

                        validate_facade(
                            value, declarations_.normal_modules.settings[module], manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::soa: {

                        schemas.push_back(value);
                        break;
                    }
                    default:
                        std::unreachable();
                }
            }();
        }
        if (!schemas.empty()) {
            validate_soa(module, schemas, manifest.types);
        }
    }

    void run_integer_domains() {
        auto const& registered{manifest_.types.registered};
        std::set<ExternalScalarId> checked;
        for (auto const& [name, type] : manifest_.types.entries) {
            auto const scalar{registered.semantics[registered.index(type)]};
            if (scalar.kind() != ExternalScalarKind::integer || !checked.insert(scalar).second) {
                continue;
            }
            auto const row{registered.integer_index(scalar)};
            validate_integer_domain(registered.integer_signedness[row] != 0,
                                    registered.integer_minimums[row],
                                    registered.integer_maximums[row],
                                    registered.integer_widths[row],
                                    registered.integer_codes[row],
                                    "External integer '" + text(name) + "'");
        }
        auto const& scalars{declarations_.integer_scalars};
        for (auto const module : manifest_.modules) {
            if (module.kind() != ModuleKind::normal) {
                continue;
            }
            auto const declarations{
                declarations_.items(declarations_.normal_modules.declarations[module])};
            for (auto const declaration : declarations) {
                if (declaration.kind() != DeclarationKind::integer_scalar) {
                    continue;
                }
                auto const row{scalars.index(declaration)};
                validate_integer_domain(scalars.signedness[row] != 0,
                                        scalars.minimum_value[row],
                                        scalars.maximum_value[row],
                                        scalars.bit_width[row],
                                        scalars.named_codes[row],
                                        "Integer scalar '" + text(scalars.name[row]) + "'");
            }
        }
    }

    void run(Manifest const& manifest) {
        if (manifest.schema_version != manifest_schema_version) {
            throw std::invalid_argument{
                std::format("Unsupported manifest schema version: {}; expected {}",
                            manifest.schema_version,
                            manifest_schema_version)};
        }
        std::set<lispb::StringId> module_names;
        std::set<std::string> output_paths;
        std::map<std::string, DeclarationHandle> generated_names;
        auto const& cpp_types{manifest.types.cpp_types};
        for (auto const& [name_id, registered] : manifest.types.entries) {
            auto const name{std::string{cpp_types.strings().view(name_id)}};
            auto const type{
                manifest.types.registered.cpp_type[manifest.types.registered.index(registered)]};
            auto const type_spelling{std::string{cpp_types.spelling(type)}};
            static_cast<void>(external_scalar_schema(manifest.types, type_spelling));
            require_value(name, "Type name");
            require_value(type_spelling, "Type '" + name + "' spelling");
            for (auto const dependency : cpp_types.dependencies(type)) {
                validate_type_dependency(cpp_types, dependency, "Type '" + name + "' dependency");
            }
            for (auto const operation : {TypeOperation::add_element,
                                         TypeOperation::remove_at_swap,
                                         TypeOperation::set_element}) {
                if (auto const spelling{cpp_types.operation(type, operation)}) {
                    require_identifier(std::string{*spelling},
                                       "Type '" + name + "' member operation");
                }
            }
        }
        for (auto const schema : manifest.modules) {
            module_ = schema;
            [&]() {
                auto const module{schema};
                switch (module.kind()) {
                    case lispb::codegen::ModuleKind::normal: {
                        validate_settings(declarations_.normal_modules.settings[module]);
                        if (!module_names
                                 .insert(declarations_.module_settings_store
                                             .name[declarations_.normal_modules.settings[module]])
                                 .second) {
                            throw std::invalid_argument{
                                "Duplicate module name: " +
                                text(declarations_.module_settings_store
                                         .name[declarations_.normal_modules.settings[module]])};
                        }
                        for (auto const& path :
                             {std::optional{
                                  declarations_.module_settings_store
                                      .header[declarations_.normal_modules.settings[module]]},
                              declarations_.module_settings_store
                                  .source[declarations_.normal_modules.settings[module]]}) {
                            if (path.has_value() &&
                                !output_paths
                                     .insert(output_path_key(std::filesystem::path{text(*path)}))
                                     .second) {
                                throw std::invalid_argument{"Duplicate generated output path: " +
                                                            text(*path)};
                            }
                        }

                        validate_normal_module(module, manifest, generated_names);
                        break;
                    }
                    case lispb::codegen::ModuleKind::settings: {
                        validate_settings(declarations_.settings_modules.settings[module]);
                        if (!module_names
                                 .insert(declarations_.module_settings_store
                                             .name[declarations_.settings_modules.settings[module]])
                                 .second) {
                            throw std::invalid_argument{
                                "Duplicate module name: " +
                                text(declarations_.module_settings_store
                                         .name[declarations_.settings_modules.settings[module]])};
                        }
                        for (auto const& path :
                             {std::optional{
                                  declarations_.module_settings_store
                                      .header[declarations_.settings_modules.settings[module]]},
                              declarations_.module_settings_store
                                  .source[declarations_.settings_modules.settings[module]]}) {
                            if (path.has_value() &&
                                !output_paths
                                     .insert(output_path_key(std::filesystem::path{text(*path)}))
                                     .second) {
                                throw std::invalid_argument{"Duplicate generated output path: " +
                                                            text(*path)};
                            }
                        }

                        validate_settings_module(module, manifest.types);
                        break;
                    }
                    case lispb::codegen::ModuleKind::umbrella: {
                        validate_settings(declarations_.umbrella_modules.settings[module]);
                        if (!module_names
                                 .insert(declarations_.module_settings_store
                                             .name[declarations_.umbrella_modules.settings[module]])
                                 .second) {
                            throw std::invalid_argument{
                                "Duplicate module name: " +
                                text(declarations_.module_settings_store
                                         .name[declarations_.umbrella_modules.settings[module]])};
                        }
                        for (auto const& path :
                             {std::optional{
                                  declarations_.module_settings_store
                                      .header[declarations_.umbrella_modules.settings[module]]},
                              declarations_.module_settings_store
                                  .source[declarations_.umbrella_modules.settings[module]]}) {
                            if (path.has_value() &&
                                !output_paths
                                     .insert(output_path_key(std::filesystem::path{text(*path)}))
                                     .second) {
                                throw std::invalid_argument{"Duplicate generated output path: " +
                                                            text(*path)};
                            }
                        }

                        if (declarations_.module_settings_store
                                .source[declarations_.umbrella_modules.settings[module]]
                                .has_value()) {
                            throw std::invalid_argument{
                                "Umbrella module '" +
                                text(declarations_.module_settings_store
                                         .name[declarations_.umbrella_modules.settings[module]]) +
                                "' must not have a source output"};
                        }
                        if (declarations_.module_settings_store
                                .namespace_name[declarations_.umbrella_modules.settings[module]]
                                .has_value()) {
                            throw std::invalid_argument{
                                "Umbrella module '" +
                                text(declarations_.module_settings_store
                                         .name[declarations_.umbrella_modules.settings[module]]) +
                                "' must not have a namespace"};
                        }
                        if (declarations_.umbrella_modules.headers[module].empty()) {
                            throw std::invalid_argument{
                                "Umbrella module '" +
                                text(declarations_.module_settings_store
                                         .name[declarations_.umbrella_modules.settings[module]]) +
                                "' must have headers"};
                        }
                        require_unique_names(
                            declarations_.umbrella_modules.headers[module],
                            "Umbrella module '" +
                                text(declarations_.module_settings_store
                                         .name[declarations_.umbrella_modules.settings[module]]) +
                                "' headers");
                        break;
                    }
                    default:
                        std::unreachable();
                }
            }();
        }
    }
  private:
    Manifest const& manifest_;
    lispb::schema::TypeGraph const& graph_;
    Declarations& declarations_;
    lispb::Strings& strings_;
    ModuleHandle module_;
};

} // namespace

void validate_integer_domains(Manifest const& manifest, lispb::schema::TypeGraph const& graph) {
    Validator{manifest, graph}.run_integer_domains();
}

void validate_manifest(Manifest const& manifest, lispb::schema::TypeGraph const& graph) {
    Validator{manifest, graph}.run(manifest);
}

} // namespace lispb::codegen::detail
