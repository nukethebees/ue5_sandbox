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
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/parameter_schemas.h>
#include <codegen/schema/physical_type_form.h>
#include <codegen/schema/record_comparison.h>
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
#include <codegen/schema/type_refs.h>
#include <codegen/schema/type_registry.h>
#include <codegen/schema/umbrella_module_schemas.h>
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
    auto resolved_type(TypeRefId id) const -> CppTypeId {
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

    void validate_type(TypeRefId reference, std::string const& context) {
        auto const type{reference};
        require_value(text(declarations_.type_refs.name[type]), context + " type");
        if (declarations_.type_refs.nested[type].has_value()) {
            require_value(*declarations_.type_refs.nested[type], context + " nested type");
        }
        static_cast<void>(graph_.resolved(module_, reference));
    }

    void validate_semantic_relation(lispb::codegen::SemanticRelationId const& relationship,
                                    std::string const& context) {
        validate_type(declarations_.semantic_relations.target[relationship], context);
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
        auto const& parameter_schemas{declarations_.parameters};
        std::vector<std::string> names;
        std::vector<std::string> type_names;
        bool has_default{};
        for (auto const parameter : declarations_.items(parameters)) {
            auto const name{text(parameter_schemas.name[parameter])};
            names.push_back(name);
            require_identifier(name, context + " parameter name");
            validate_type(parameter_schemas.type[parameter], context + " parameter '" + name + "'");
            type_names.push_back(std::string{
                types.cpp_types.spelling(resolved_type(parameter_schemas.type[parameter]))});
            if (parameter_schemas.default_value[parameter].has_value()) {
                require_non_blank_value(*parameter_schemas.default_value[parameter],
                                        context + " parameter '" + name + "' default");
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
        auto const& type_refs{declarations_.type_refs};

        if (auto const columns{functions.append_columns[function]}) {
            if (!soa_owner || functions.is_static[function] || functions.is_const[function] ||
                functions.is_constexpr[function] || !functions.body_lines[function].empty()) {
                throw std::invalid_argument{context + " append-columns requires a mutable owner "
                                                      "function without a body or constexpr"};
            }
            if (columns->size() != functions.parameters[function].size()) {
                throw std::invalid_argument{context +
                                            " append-columns requires one column per parameter"};
            }
            require_unique_names(*columns, context + " append columns");
            for (auto const parameter : declarations_.items(functions.parameters[function])) {
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
        if (functions.is_constexpr[function] &&
            (functions.body_lines[function].empty() || functions.definition_in_source[function])) {
            throw std::invalid_argument{context +
                                        " constexpr function requires a generated header body"};
        }
        if (functions.is_static[function] && functions.is_const[function]) {
            throw std::invalid_argument{context + " must not be both static and const"};
        }
        if (functions.is_inline[function] && functions.definition_in_source[function]) {
            throw std::invalid_argument{context +
                                        " must not be both inline and defined in the source"};
        }
        if (functions.template_parameters[function] && functions.definition_in_source[function]) {
            throw std::invalid_argument{context +
                                        " function templates must be defined in the header"};
        }
        if (functions.template_parameters[function]) {
            require_non_blank_value(*functions.template_parameters[function],
                                    context + " template parameters");
        }
        if (functions.requires_clause[function]) {
            require_non_blank_value(*functions.requires_clause[function],
                                    context + " requires clause");
        }

        auto const return_type{functions.return_type[function]};
        validate_type(return_type, context + " return");
        if (functions.trailing_return_type[function]) {
            if (view(type_refs.name[return_type]) != "auto" ||
                !view(type_refs.suffix[return_type]).empty() || type_refs.nested[return_type]) {
                throw std::invalid_argument{context +
                                            " trailing return type requires an auto return"};
            }
            validate_type(*functions.trailing_return_type[function], context + " trailing return");
        }
        auto const effective_return{functions.trailing_return_type[function]
                                        ? *functions.trailing_return_type[function]
                                        : return_type};
        auto const result{graph_.resolved(module_, effective_return)};
        auto const& occurrences{graph_.occurrences()};
        auto const occurrence_row{occurrences.index(result)};
        if (functions.append_columns[function] &&
            types.cpp_types.strings().view(occurrences.object_spelling[occurrence_row]) == "void") {
            throw std::invalid_argument{context +
                                        " append-columns must return a row index or row ID"};
        }
        if (functions.is_nodiscard[function] &&
            occurrences.physical_form[occurrence_row] == PhysicalTypeForm::value &&
            types.cpp_types.strings().view(occurrences.object_spelling[occurrence_row]) == "void") {
            throw std::invalid_argument{context + " nodiscard function must not return void"};
        }

        if (!signatures
                 .insert(
                     signature(functions.name[function],
                               parameter_type_names(functions.parameters[function], types, context),
                               functions.is_const[function]))
                 .second) {
            throw std::invalid_argument{"Duplicate " + context + " signature"};
        }
        for (auto const dependency : declarations_.items(functions.dependencies[function])) {
            validate_dependency(dependency, types, context);
        }
    }

    void validate_settings(lispb::codegen::ModuleSettingsId const& settings) {
        auto const& modules{declarations_.module_settings_store};

        require_value(text(modules.name[settings]), "Module name");
        auto emits_cpp{module_.kind() != ModuleKind::normal};
        if (!emits_cpp) {
            for (auto const declaration :
                 declarations_.items(declarations_.normal_modules.declarations[module_])) {
                emits_cpp = emits_cpp || !generated_cpp_names(declarations_, declaration).empty();
            }
        }
        if (emits_cpp && text(modules.header[settings]).empty()) {
            throw std::invalid_argument{"Module '" + text(modules.name[settings]) +
                                        "' header must not be empty"};
        }
        if (modules.source[settings].has_value() && view(*modules.source[settings]).empty()) {
            throw std::invalid_argument{"Module '" + text(modules.name[settings]) +
                                        "' source must not be empty"};
        }
        if (modules.header_include[settings].has_value()) {
            require_value(*modules.header_include[settings],
                          "Module '" + text(modules.name[settings]) + "' header include");
        }
        if (modules.namespace_name[settings].has_value()) {
            require_qualified_identifier(*modules.namespace_name[settings],
                                         "Module '" + text(modules.name[settings]) + "' namespace");
        }
        require_unique_names(modules.include_order[settings],
                             "Module '" + text(modules.name[settings]) + "' include order");
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
        auto const& enums{declarations_.enums};
        auto const& enumerators{declarations_.enumerators};
        auto const& projections{declarations_.enum_unreal_projections};

        auto const name{text(enums.name[schema])};
        require_identifier(name, "Enum name");
        auto const enum_context{"Enum '" + name + "'"};

        auto const& domains{graph_.enum_domains()};
        auto const domain{domains.find(schema)};
        auto const domain_row{domains.index(domain)};
        std::optional<EnumStorageDomain> storage_domain;
        auto const underlying_reference{enums.cpp_projection[schema] &&
                                                enums.cpp_projection[schema]->storage
                                            ? enums.cpp_projection[schema]->storage
                                            : enums.underlying_type[schema]};
        if (underlying_reference) {
            validate_type(*underlying_reference, enum_context + " underlying");
            auto const underlying{resolved_type(*underlying_reference)};
            storage_domain = enum_storage_domain(std::string{types.cpp_types.spelling(underlying)});
        } else {
            auto const derived{lispb::schema::derive_enum_storage_requirement(
                domains, domain, enums.bit_width[schema])};
            if (!derived.has_value() && enums.cpp_projection[schema]) {
                throw std::invalid_argument{
                    enum_context +
                    " cannot derive C++ backing storage because its semantic signedness or width "
                    "is unknown"};
            }
            if (derived) {
                storage_domain = EnumStorageDomain{.bit_width = derived->bit_width,
                                                   .is_unsigned = !derived->signedness};
            }
        }
        if (enums.signedness[schema] != schema::EnumSignedness::unspecified) {
            auto const issue_count{domains.issue_count(domain)};
            for (std::uint32_t issue{}; issue < issue_count; ++issue) {
                if (domains.issue_severity(domain, issue) ==
                    schema::EnumDomainIssueSeverity::error) {
                    throw std::invalid_argument{enum_context + ": " +
                                                domains.issue_message(domain, issue)};
                }
            }
        }
        if (enums.bit_width[schema].has_value()) {
            if (*enums.bit_width[schema] == 0 || *enums.bit_width[schema] > 64) {
                throw std::invalid_argument{enum_context +
                                            " bit width must be between 1 and 64 bits"};
            }
            if (domains.minimum_required_bits[domain_row].has_value() &&
                *domains.minimum_required_bits[domain_row] > *enums.bit_width[schema]) {
                throw std::invalid_argument{
                    std::format("Enum '{}' known value domain requires at least {} bits but its "
                                "semantic bit width is {}",
                                name,
                                *domains.minimum_required_bits[domain_row],
                                *enums.bit_width[schema])};
            }
        }
        validate_export_specifier(enums.export_specifier[schema],
                                  enum_context + " export specifier");
        if (enums.unreal_projection[schema].has_value()) {
            if (!enums.cpp_projection[schema]) {
                throw std::invalid_argument{"Enum Unreal projection requires a C++ projection"};
            }
            auto const projection{*enums.unreal_projection[schema]};
            require_identifier(text(projections.name[projection]),
                               enum_context + " Unreal projection name");
            require_value(text(projections.header[projection]),
                          enum_context + " Unreal projection header");
            require_value(text(projections.header_include[projection]),
                          enum_context + " Unreal projection header include");
            require_value(text(projections.conversion_header[projection]),
                          enum_context + " Unreal projection conversion header");
            require_value(projections.native_header_include[projection],
                          enum_context + " native enum header include");
            if (!enums.native_api[schema] || enums.reflection[schema] != EnumReflection::none) {
                throw std::invalid_argument{enum_context +
                                            " Unreal projection requires a native plain enum"};
            }
            if (projections.reflection[projection] == EnumReflection::none) {
                throw std::invalid_argument{enum_context + " Unreal projection must be reflected"};
            }
        }
        if (!enums.native_api[schema] && !enums.helpers[schema].empty()) {
            throw std::invalid_argument{enum_context + " helpers require native-api"};
        }
        std::set<NativeEnumHelper> helpers;
        auto const helper_count{
            ioj::enum_strings<NativeEnumHelper, ioj::Serialized>.offsets.size() - 1};
        for (auto const helper : declarations_.items(enums.helpers[schema])) {
            if (std::to_underlying(helper) >= helper_count || !helpers.insert(helper).second) {
                throw std::invalid_argument{enum_context + " has an invalid or duplicate helper"};
            }
        }
        if (helpers.contains(NativeEnumHelper::formatter) &&
            !helpers.contains(NativeEnumHelper::to_string)) {
            throw std::invalid_argument{
                std::format("Enum '{}' formatter requires the to-string helper", name)};
        }
        if (enums.native_api[schema] && enums.reflection[schema] != EnumReflection::none) {
            throw std::invalid_argument{"Native enum '" + name + "' cannot use Unreal reflection"};
        }
        if (enums.native_api[schema] &&
            (!enums.conversions[schema].empty() || (enums.export_specifier[schema].value != 0))) {
            throw std::invalid_argument{"Native enum '" + name +
                                        "' cannot use Unreal enum generation options"};
        }
        if (enums.reflection[schema] != EnumReflection::none &&
            declarations_.module_settings_store
                .namespace_name[declarations_.normal_modules.settings[module]]
                .has_value()) {
            throw std::invalid_argument{"Reflected enum '" + name +
                                        "' must be declared at global scope"};
        }
        if (enums.values[schema].empty()) {
            throw std::invalid_argument{enum_context + " must have values"};
        }
        std::vector<std::string> value_names;
        std::vector<std::string> serialized_names;
        auto const enum_conversions{declarations_.items(enums.conversions[schema])};
        auto const enum_values{declarations_.items(enums.values[schema])};
        auto const value_count{enum_values.size()};
        auto const has_string_lookup{
            helpers.contains(NativeEnumHelper::to_string) ||
            helpers.contains(NativeEnumHelper::to_display_string) ||
            helpers.contains(NativeEnumHelper::to_serialized_string) ||
            std::ranges::any_of(enum_conversions, [](EnumConversion conversion) {
                return conversion != EnumConversion::try_parse_serialized;
            })};
        if (has_string_lookup) {
            auto const valid_codes{domains.distinct_code_count[domain_row] == value_count && [&] {
                for (std::size_t index{}; index < value_count; ++index) {
                    auto const code{domains.code(domain, index)};
                    if (!code || code->negative || code->magnitude >= value_count) {
                        return false;
                    }
                }
                return true;
            }()};
            if (!valid_codes) {
                throw std::invalid_argument{
                    std::format("Enum '{}' string conversions require known, unique, zero-based "
                                "contiguous codes",
                                name)};
            }
        }
        auto const has_serialized_conversion{
            helpers.contains(NativeEnumHelper::to_serialized_string) ||
            helpers.contains(NativeEnumHelper::try_parse_serialized) ||
            std::find(enum_conversions.begin(),
                      enum_conversions.end(),
                      EnumConversion::lex_to_serialized_string) != enum_conversions.end() ||
            std::find(enum_conversions.begin(),
                      enum_conversions.end(),
                      EnumConversion::try_parse_serialized) != enum_conversions.end()};
        for (std::size_t index{}; index < value_count; ++index) {
            auto const value{enum_values[index]};
            require_identifier(text(enumerators.name[value]), enum_context + " value name");
            value_names.push_back(text(enumerators.name[value]));
            if (enumerators.initializer[value].has_value()) {
                require_value(*enumerators.initializer[value],
                              enum_context + " value '" + text(enumerators.name[value]) +
                                  "' initializer");
            }
            auto const code{domains.code(domain, index)};
            if (storage_domain.has_value() && code.has_value() &&
                !enum_code_fits(*code, *storage_domain)) {
                throw std::invalid_argument{enum_context + " value '" +
                                            text(enumerators.name[value]) +
                                            "' does not fit its underlying type"};
            }
            if (enumerators.display_name[value].has_value()) {
                require_value(*enumerators.display_name[value],
                              enum_context + " value '" + text(enumerators.name[value]) +
                                  "' display name");
            }
            if (enumerators.serialized_name[value].has_value()) {
                require_value(*enumerators.serialized_name[value],
                              enum_context + " value '" + text(enumerators.name[value]) +
                                  "' serialized name");
                serialized_names.push_back(text(*enumerators.serialized_name[value]));
            } else if (has_serialized_conversion &&
                       (!enums.count[schema].has_value() ||
                        enumerators.name[value] != *enums.count[schema])) {
                throw std::invalid_argument{enum_context + " value '" +
                                            text(enumerators.name[value]) +
                                            "' requires a serialized name"};
            }
            if (enumerators.hidden[value] && enums.reflection[schema] == EnumReflection::none &&
                !enums.unreal_projection[schema].has_value() &&
                (!enums.count[schema].has_value() ||
                 enumerators.name[value] != *enums.count[schema])) {
                throw std::invalid_argument{"Plain enum '" + name + "' value '" +
                                            text(enumerators.name[value]) + "' cannot be hidden"};
            }
        }
        require_unique_names(value_names, enum_context + " values");
        require_unique_names(serialized_names, enum_context + " serialized names");

        auto count_value{enum_values.end()};
        if (enums.count[schema].has_value()) {
            require_identifier(*enums.count[schema], enum_context + " count");
            count_value =
                std::find_if(enum_values.begin(), enum_values.end(), [&](auto const value) {
                    return enumerators.name[value] == *enums.count[schema];
                });
            if (count_value == enum_values.end()) {
                throw std::invalid_argument{enum_context + " count '" + text(*enums.count[schema]) +
                                            "' does not name an enum value"};
            }
            if (std::next(count_value) != enum_values.end()) {
                throw std::invalid_argument{enum_context + " count must be the final enum value"};
            }
            if (count_value == enum_values.begin()) {
                throw std::invalid_argument{enum_context + " count must follow at least one value"};
            }
        }
        if (enums.enum_array[schema] ||
            (enums.unreal_projection[schema].has_value() &&
             projections.enum_array[*enums.unreal_projection[schema]])) {
            if (enums.count[schema].has_value()) {
                throw std::invalid_argument{
                    "Enum-array enum '" + name +
                    "' must use its generated count instead of a count sentinel"};
            }

            // Certify direct indexing before emitting the count specialization.
            auto const values{domains.values(domain)};
            auto const domain_value_count{values.size()};
            for (std::size_t index{}; index < domain_value_count; ++index) {
                auto const value{values[index]};
                auto const code{domains.code(domain, index)};
                auto const value_row{enumerators.index(value)};
                auto const value_name{enumerators.name[value_row]};
                if (!code.has_value() || code->negative || code->magnitude != index) {
                    throw std::invalid_argument{std::format(
                        "Enum-array enum '{}' value '{}' must have the zero-based contiguous code "
                        "{}",
                        name,
                        text(value_name),
                        index)};
                }
                if (enumerators.hidden[value_row]) {
                    throw std::invalid_argument{"Enum-array enum '" + name + "' value '" +
                                                text(value_name) + "' must not be hidden"};
                }
            }
        }

        std::set<EnumConversion> conversions;
        for (auto const conversion : enum_conversions) {
            if (!conversions.insert(conversion).second) {
                throw std::invalid_argument{enum_context + " contains duplicate conversion"};
            }
        }
        if (!enums.conversions[schema].empty() &&
            !declarations_.module_settings_store
                 .source[declarations_.normal_modules.settings[module]]
                 .has_value()) {
            throw std::invalid_argument{enum_context + " conversions require a source output"};
        }
    }

    void validate_record(lispb::codegen::DeclarationHandle const& record,
                         TypeRegistry const& types) {
        auto const& records{declarations_.records};
        auto const& members{declarations_.child_occurrences};
        auto const& functions{declarations_.functions};

        require_identifier(text(records.name[record]), "Record name");
        auto const record_context{"Record '" + text(records.name[record]) + "'"};
        validate_export_specifier(records.export_specifier[record],
                                  record_context + " export specifier");
        if (records.layout[record] == LayoutKind::native && records.cpp_projection[record] &&
            records.cpp_projection[record]->storage) {
            throw std::invalid_argument{
                "Native record C++ projection cannot specify integer storage"};
        }
        if (records.layout[record] == LayoutKind::packed &&
            (records.generate_visitor[record] || records.s7_tag[record] ||
             records.constraint_policy[record] || !records.functions[record].empty())) {
            throw std::invalid_argument{"Packed record does not support native record visitors, "
                                        "constraints, readers, or methods"};
        }
        if (records.generate_visitor[record]) {
            auto const record_name{text(records.name[record]) +
                                   (records.constraint_policy[record] ? "Value" : "")};
            if (record_name == "Field" || record_name == "visit_field" || record_name == "Self" ||
                record_name == "Visitor" || record_name == "Selected" ||
                record_name == "get_field" || record_name == "invoke_field") {
                throw std::invalid_argument{"Record visitor conflicts with record name: " +
                                            record_name};
            }
            auto const check_name{[&](StringId name) {
                auto const value{text(name)};
                if (value == "Field" || value == "visit_field" || value == "get_field" ||
                    value == "invoke_field") {
                    throw std::invalid_argument{"Record visitor reserves the name: " + value};
                }
            }};
            for (auto const id : declarations_.items(records.members[record])) {
                check_name(members.name[id]);
            }
            for (auto const id : declarations_.items(records.functions[record])) {
                check_name(functions.name[id]);
            }
        }
        if (records.constraint_policy[record]) {
            require_qualified_identifier(*records.constraint_policy[record],
                                         "Record constraint policy");
        }
        for (auto const id : declarations_.items(records.members[record])) {
            auto const member{id};
            if (members.s7[member].name) {
                require_non_blank_value(*members.s7[member].name, "S7 member name");
            }
        }
        std::vector<std::string> member_names;
        member_names.reserve(records.members[record].size());
        for (auto const member : declarations_.items(records.members[record])) {
            auto const context{record_context + " member '" + text(members.name[member]) + "'"};
            require_identifier(text(members.name[member]), record_context + " member name");
            member_names.push_back(text(members.name[member]));
            if (members.kind[member] == ChildOccurrenceKind::reserved) {
                if (records.layout[record] != LayoutKind::packed || !members.bits[member] ||
                    *members.bits[member] == 0) {
                    throw std::invalid_argument{
                        context + " reserved regions require a positive width and packed layout"};
                }
                continue;
            }
            validate_type(members.type[member], context);
            if (members.case_label[member]) {
                throw std::invalid_argument{context + " :case is only valid on union alternatives"};
            }
            if (records.layout[record] == LayoutKind::packed &&
                (members.s7[member].minimum || members.s7[member].maximum)) {
                throw std::invalid_argument{
                    context + " packed member bounds belong to its scalar definition"};
            }
            if (members.relationship[member].has_value()) {
                validate_semantic_relation(*members.relationship[member],
                                           context + " relationship");
            }
            if (members.count[member] == 0) {
                throw std::invalid_argument{context + " count must be greater than zero"};
            }
        }
        require_unique_names(member_names, record_context + " members");
        if (records.comparison_noexcept[record] &&
            records.comparison[record] == RecordComparison::none) {
            throw std::invalid_argument{"Record comparison-noexcept requires a comparison"};
        }
        for (auto const member : declarations_.items(records.members[record])) {
            if (members.initializer[member] &&
                view(*members.initializer[member]).find_first_of(";\r\n#") != std::string::npos) {
                throw std::invalid_argument{
                    "Record initializer must be a brace initializer expression list"};
            }
        }
        std::set<std::string> signatures;
        for (auto const function : declarations_.items(records.functions[record])) {
            auto const function_name{text(functions.name[function])};
            auto const context{record_context + " function '" + function_name + "'"};
            if (function_name != "operator[]" && function_name != "operator+" &&
                function_name != "operator-" && function_name != "operator+=" &&
                function_name != "operator-=") {
                require_identifier(function_name, context);
            }
            if (function_name == text(records.name[record]) ||
                std::ranges::find(member_names, function_name) != member_names.end()) {
                throw std::invalid_argument{context + " collides with a member or owning type"};
            }
            validate_function(function, types, context, signatures);
            if (functions.definition_in_source[function] ||
                functions.template_parameters[function] || functions.requires_clause[function]) {
                throw std::invalid_argument{
                    context + " requires an ordinary header method or bodyless declaration"};
            }
        }
    }

    void validate_union(lispb::codegen::DeclarationHandle const& schema) {
        require_identifier(text(declarations_.unions.name[schema]), "Union name");
        auto const& unions{declarations_.unions};
        if (unions.layout[schema] == LayoutKind::native && unions.cpp_projection[schema] &&
            unions.cpp_projection[schema]->storage) {
            throw std::invalid_argument{
                "Native union C++ projection cannot specify integer storage"};
        }
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
                               text(declarations_.child_occurrences.name[alternative]) + "'"};
            require_identifier(text(declarations_.child_occurrences.name[alternative]),
                               "Union '" + text(declarations_.unions.name[schema]) +
                                   "' alternative name");
            validate_type(declarations_.child_occurrences.type[alternative], context);
            auto const& s7{declarations_.child_occurrences.s7[alternative]};
            if (s7.minimum || s7.maximum) {
                throw std::invalid_argument{
                    context + " union alternative bounds belong to its scalar definition"};
            }
            if (declarations_.child_occurrences.count[alternative] == 0) {
                throw std::invalid_argument{context + " count must be greater than zero"};
            }
            alternative_names.push_back(text(declarations_.child_occurrences.name[alternative]));
        }
        require_unique_names(alternative_names,
                             "Union '" + text(declarations_.unions.name[schema]) +
                                 "' alternatives");
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
        auto const& codes{declarations_.packed_named_codes};
        for (auto const code : declarations_.items(named_codes)) {
            auto const name{codes.name[code]};
            auto const value{codes.value[code]};
            auto const sentinel{codes.sentinel[code]};
            auto const code_context{context + " code '" + text(name) + "'"};
            require_identifier(text(name), code_context + " name");
            if (!code_names.insert(name).second) {
                throw std::invalid_argument{context + " has duplicate code: " + text(name)};
            }
            if (!code_values.emplace(value.negative, value.magnitude).second) {
                throw std::invalid_argument{
                    context + " has duplicate code value: " + format_packed_integer(value)};
            }
            if (!signedness && value.negative) {
                throw std::invalid_argument{code_context + " is negative in an unsigned domain"};
            }
            auto const inside_live_range{packed_integer_less_equal(minimum_value, value) &&
                                         packed_integer_less_equal(value, maximum_value)};
            if (sentinel == inside_live_range) {
                throw std::invalid_argument{
                    code_context + (sentinel ? " sentinel must be outside the live range"
                                             : " non-sentinel must be inside the live range")};
            }
            if (packed_integer_less(value, required_minimum)) {
                required_minimum = value;
            }
            if (packed_integer_less(required_maximum, value)) {
                required_maximum = value;
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
        auto const& representations{declarations_.integer_scalars};
        auto const context{"Integer scalar '" + text(representations.name[scalar]) + "'"};
        require_identifier(text(representations.name[scalar]), "Integer scalar name");

        if (representations.relationship[scalar].has_value()) {
            validate_semantic_relation(*representations.relationship[scalar],
                                       context + " relationship");
            auto const relation_kind{
                declarations_.semantic_relations.kind[*representations.relationship[scalar]]};
            auto const requires_unsigned{relation_kind == SemanticRelationKind::index_into ||
                                         relation_kind == SemanticRelationKind::count_of ||
                                         relation_kind == SemanticRelationKind::offset_into};
            if (requires_unsigned && representations.signedness[scalar]) {
                throw std::invalid_argument{
                    std::format("{} relationship '{:s}' requires an unsigned integer scalar",
                                context,
                                relation_kind)};
            }
        }

        auto const projection{representations.cpp_projection[scalar]};
        if (projection && projection->storage) {
            validate_type(*projection->storage, context + " C++ storage");
            auto const cpp_type{resolved_type(*projection->storage)};
            if (!packed_unsigned_width(types.cpp_types.spelling(cpp_type))) {
                throw std::invalid_argument{context +
                                            " C++ storage must be an unsigned fixed-width integer"};
            }
        }
    }

    auto require_integer_domain(TypeRefId reference, std::string const& context)
        -> schema::TypeDefinitionId {
        validate_type(reference, context + " source");
        auto const source{find_integer_domain(reference, graph_, module_)};
        if (!source.has_value()) {
            throw std::invalid_argument{context + " source must have a declared integer domain"};
        }
        return *source;
    }

    void validate_linear_quantized(lispb::codegen::DeclarationHandle const& representation) {
        auto const& representations{declarations_.linear_quantizeds};
        auto const& integer_scalars{graph_.definitions().integer_scalars};
        auto const context{"Linear quantization '" + text(representations.name[representation]) +
                           "'"};
        require_identifier(text(representations.name[representation]), "Linear quantization name");

        auto const source{require_integer_domain(representations.source[representation], context)};
        auto const bit_width{representations.bit_width[representation]};
        auto const reserved_codes{representations.reserved_codes[representation]};

        if (!packed_integer_less(integer_scalars.minimum_value[integer_scalars.index(source)],
                                 integer_scalars.maximum_value[integer_scalars.index(source)])) {
            throw std::invalid_argument{context +
                                        " source range must contain at least two distinct values"};
        }
        if (bit_width == 0 || bit_width > 64) {
            throw std::invalid_argument{context + " bit width must be in the range 1..64"};
        }
        auto const maximum_reserved{bit_width == 64
                                        ? (std::numeric_limits<std::uint64_t>::max)() - 1
                                        : (std::uint64_t{1} << bit_width) - 2};
        if (reserved_codes > maximum_reserved) {
            throw std::invalid_argument{
                std::format("{} reserves {} codes but at least two usable codes are required",
                            context,
                            reserved_codes)};
        }
    }

    void validate_integer_varint(lispb::codegen::DeclarationHandle const& representation) {
        auto const& representations{declarations_.integer_varints};
        auto const& integer_scalars{graph_.definitions().integer_scalars};
        auto const context{"Integer varint '" + text(representations.name[representation]) + "'"};
        require_identifier(text(representations.name[representation]), "Integer varint name");

        auto const source{require_integer_domain(representations.source[representation], context)};
        if (representations.encoding[representation] == IntegerVarintEncoding::unsigned_varint &&
            integer_scalars.signedness[integer_scalars.index(source)]) {
            throw std::invalid_argument{context + " unsigned encoding requires an unsigned source"};
        }
        if (representations.encoding[representation] != IntegerVarintEncoding::unsigned_varint &&
            !integer_scalars.signedness[integer_scalars.index(source)]) {
            throw std::invalid_argument{context +
                                        " signed and zigzag encodings require a signed source"};
        }
    }

    void validate_fixed_point(lispb::codegen::DeclarationHandle const& representation) {
        auto const& representations{declarations_.fixed_points};
        auto const context{"Fixed-point representation '" +
                           text(representations.name[representation]) + "'"};
        require_identifier(text(representations.name[representation]),
                           "Fixed-point representation name");

        auto const total_bits{representations.total_bits[representation]};
        auto const fractional_bits{representations.fractional_bits[representation]};
        auto const signedness{representations.signedness[representation]};

        if (total_bits == 0 || total_bits > 64) {
            throw std::invalid_argument{context + " total width must be in the range 1..64"};
        }
        auto const maximum_fractional_bits{signedness ? total_bits - 1 : total_bits};
        if (fractional_bits > maximum_fractional_bits) {
            throw std::invalid_argument{std::format("{} fractional width must be at most {}{}",
                                                    context,
                                                    maximum_fractional_bits,
                                                    signedness ? " so one sign bit remains" : "")};
        }

        auto const parse_bound = [&](std::optional<StringId> value,
                                     std::string_view name) -> std::optional<PackedIntegerValue> {
            if (!value) {
                return std::nullopt;
            }
            auto parsed{parse_fixed_point_value(view(*value), fractional_bits)};
            if (!parsed) {
                throw std::invalid_argument{context + " " + std::string{name} + ": " +
                                            parsed.error()};
            }
            return *parsed;
        };
        auto const minimum{parse_bound(representations.minimum_value[representation], "minimum")};
        auto const maximum{parse_bound(representations.maximum_value[representation], "maximum")};
        auto const fits = [&](PackedIntegerValue const value) {
            return signedness ? packed_integer_fits_signed(value, total_bits)
                              : packed_integer_fits_unsigned(value, total_bits);
        };
        if ((minimum.has_value() && !fits(*minimum)) || (maximum.has_value() && !fits(*maximum))) {
            throw std::invalid_argument{context + " bounds must fit the representation width"};
        }
        if (minimum.has_value() && maximum.has_value() && packed_integer_less(*maximum, *minimum)) {
            throw std::invalid_argument{context + " maximum must be at least minimum"};
        }
    }

    void validate_mini_float(lispb::codegen::DeclarationHandle const& representation) {
        auto const& representations{declarations_.mini_floats};
        auto const context{"Mini-float representation '" +
                           text(representations.name[representation]) + "'"};
        require_identifier(text(representations.name[representation]),
                           "Mini-float representation name");

        auto const sign_bits{representations.sign_bits[representation]};
        auto const exponent_bits{representations.exponent_bits[representation]};
        auto const significand_bits{representations.significand_bits[representation]};
        auto const exponent_bias{representations.exponent_bias[representation]};

        if (sign_bits > 1) {
            throw std::invalid_argument{context + " sign width must be zero or one"};
        }
        if (exponent_bits < 2 || exponent_bits > 15) {
            throw std::invalid_argument{context + " exponent width must be in the range 2..15"};
        }
        if (significand_bits > 62) {
            throw std::invalid_argument{context + " significand width must be in the range 0..62"};
        }
        auto const total_bits{sign_bits + exponent_bits + significand_bits};
        if (total_bits > 64) {
            throw std::invalid_argument{context + " total width must not exceed 64 bits"};
        }
        if (exponent_bias < -32'768 || exponent_bias > 32'767) {
            throw std::invalid_argument{context +
                                        " exponent bias must be in the range -32768..32767"};
        }
    }

    void validate_optional_sentinel(lispb::codegen::DeclarationHandle const& representation) {
        auto const& representations{declarations_.optional_sentinels};
        auto const& integer_scalars{graph_.definitions().integer_scalars};
        auto const context{"Optional sentinel representation '" +
                           text(representations.name[representation]) + "'"};
        require_identifier(text(representations.name[representation]),
                           "Optional sentinel representation name");

        auto const source{require_integer_domain(representations.source[representation], context)};
        auto const sentinel{representations.sentinel[representation]};
        auto const named_codes{
            declarations_.items(integer_scalars.named_codes[integer_scalars.index(source)])};
        auto const& codes{declarations_.packed_named_codes};
        require_identifier(text(sentinel), context + " sentinel code");
        auto const code{
            std::ranges::find(named_codes, sentinel, [&](auto id) { return codes.name[id]; })};
        if (code == named_codes.end()) {
            auto const has_sentinel{
                std::ranges::any_of(named_codes, [&](auto id) { return codes.sentinel[id]; })};
            throw std::invalid_argument{
                has_sentinel ? context + " names unknown source code '" + text(sentinel) + "'"
                             : context + " source has no named sentinel codes"};
        }
        if (!codes.sentinel[*code]) {
            throw std::invalid_argument{context + " code '" + text(sentinel) +
                                        "' is not a sentinel"};
        }
    }

    void validate_optional_presence_bit(lispb::codegen::DeclarationHandle const& representation) {
        auto const& representations{declarations_.optional_presence_bits};
        auto const context{"Optional presence-bit representation '" +
                           text(representations.name[representation]) + "'"};
        require_identifier(text(representations.name[representation]),
                           "Optional presence-bit representation name");

        require_integer_domain(representations.source[representation], context);
    }

    void validate_soa(lispb::codegen::ModuleHandle const& module,
                      std::span<lispb::codegen::DeclarationHandle const> schemas,
                      TypeRegistry const& types) {
        auto const& soas{declarations_.soas};
        auto const& members{declarations_.soa_members};
        auto const& functions{declarations_.functions};

        for (auto const& schema : schemas) {
            auto const schema_name{text(soas.name[schema])};
            require_identifier(schema_name, "SOA name");
            auto const context{"SOA '" + schema_name + "'"};
            require_identifier(soa_namespace_name(schema_name), context + " namespace");
            if (soas.layout_only[schema] && !soas.functions[schema].empty()) {
                throw std::invalid_argument{context +
                                            " layout-only has no owner; use view functions"};
            }

            if (soas.field_mask_name[schema].has_value() !=
                soas.field_enum_name[schema].has_value()) {
                throw std::invalid_argument{
                    "SOA '" + schema_name +
                    "' must specify both field-mask-name and field-enum-name"};
            }
            if (soas.field_mask_name[schema].has_value()) {
                require_identifier(*soas.field_mask_name[schema],
                                   "SOA '" + schema_name + "' field mask");
                require_identifier(*soas.field_enum_name[schema],
                                   "SOA '" + schema_name + "' field enum");
            }
            if (soas.members[schema].empty()) {
                throw std::invalid_argument{"SOA '" + schema_name + "' must have members"};
            }
            std::vector<std::string> member_names;
            if (soas.equivalent_constructor[schema] && !soas.equivalent_type[schema]) {
                throw std::invalid_argument{
                    "SOA equivalent constructor requires an equivalent type"};
            }
            auto mapped_members{std::size_t{}};
            int mask_storage_member_count{};
            int mask_field_count{};
            for (auto const member : declarations_.items(soas.members[schema])) {
                auto const member_row{members.index(member)};
                if (members.equivalent_member[member_row]) {
                    ++mapped_members;
                    if (!soas.equivalent_type[schema]) {
                        throw std::invalid_argument{
                            "SOA equivalent member mapping requires an equivalent type"};
                    }
                } else if (members.equivalent_accessor[member_row]) {
                    throw std::invalid_argument{
                        "SOA equivalent accessor requires a member mapping"};
                }
                auto const context{"SOA '" + schema_name + "' member '" +
                                   text(members.name[member]) + "'"};
                member_names.push_back(text(members.name[member]));
                require_identifier(text(members.name[member]),
                                   "SOA '" + schema_name + "' member name");
                reject_generated_name_collision(
                    text(members.name[member]), "SOA '" + schema_name + "' member name", true);
                validate_type(members.type[member], context);
                if (members.relationship[member].has_value()) {
                    validate_semantic_relation(*members.relationship[member],
                                               context + " relationship");
                }
                if (soas.field_mask_name[schema].has_value() &&
                    declarations_.type_refs.name[members.type[member]] ==
                        *soas.field_mask_name[schema]) {
                    ++mask_storage_member_count;
                    if (members.kind[member] != SoaMemberKind::array) {
                        throw std::invalid_argument{"SOA '" + schema_name +
                                                    "' field-mask storage must be an array member"};
                    }
                }
                if (!members.mask_dimensions[member].empty() && !members.mask_field[member]) {
                    throw std::invalid_argument{"SOA '" + schema_name + "' member '" +
                                                text(members.name[member]) +
                                                "' has mask dimensions but is not a mask field"};
                }
                if (members.mask_field[member]) {
                    ++mask_field_count;
                    if (!soas.field_mask_name[schema].has_value()) {
                        throw std::invalid_argument{
                            "SOA '" + schema_name + "' member '" + text(members.name[member]) +
                            "' is a mask field without a generated field mask"};
                    }
                    if (members.kind[member] != SoaMemberKind::array) {
                        throw std::invalid_argument{"SOA '" + schema_name + "' mask field '" +
                                                    text(members.name[member]) +
                                                    "' must be an array member"};
                    }
                    std::set<std::string> dimension_names;
                    for (auto const dimension_id :
                         declarations_.items(members.mask_dimensions[member])) {
                        auto const dimension{dimension_id};
                        require_identifier(
                            text(declarations_.soa_mask_dimensions.index_name[dimension]),
                            "SOA '" + schema_name + "' mask dimension index");
                        require_value(text(declarations_.soa_mask_dimensions.extent[dimension]),
                                      "SOA '" + schema_name + "' mask dimension extent");
                        if (!dimension_names
                                 .insert(
                                     text(declarations_.soa_mask_dimensions.index_name[dimension]))
                                 .second) {
                            throw std::invalid_argument{"SOA '" + schema_name + "' mask field '" +
                                                        text(members.name[member]) +
                                                        "' has duplicate dimension names"};
                        }
                    }
                }
            }
            if (soas.field_mask_name[schema].has_value() && mask_storage_member_count != 1) {
                throw std::invalid_argument{
                    "SOA '" + schema_name +
                    "' must have exactly one array member of its field-mask type"};
            }
            if (soas.field_mask_name[schema].has_value() && mask_field_count == 0) {
                throw std::invalid_argument{
                    "SOA '" + schema_name +
                    "' generated field mask must contain at least one field"};
            }
            if (mapped_members != 0 && mapped_members != member_names.size()) {
                throw std::invalid_argument{
                    "SOA equivalent member mappings must cover every column"};
            }
            require_unique_names(member_names, "SOA '" + schema_name + "' members");
            validate_export_specifier(soas.export_specifier[schema],
                                      "SOA '" + schema_name + "' export specifier");
            for (auto const declaration : declarations_.items(soas.using_declarations[schema])) {
                require_non_blank_value(declaration, "SOA '" + schema_name + "' using declaration");
            }
            if (soas.equivalent_type[schema].has_value()) {
                validate_type(*soas.equivalent_type[schema],
                              "SOA '" + schema_name + "' equivalent");
            }
            auto validate_functions = [&](SchemaRange<FunctionSchemaId> function_ids,
                                          std::set<std::string>& function_signatures,
                                          bool const read_only,
                                          bool const owner = false) {
                for (auto const function : declarations_.items(function_ids)) {
                    auto const context{"SOA '" + schema_name + "' function '" +
                                       text(functions.name[function]) + "'"};
                    validate_function(function, types, context, function_signatures, owner);
                    if (auto const columns{functions.append_columns[function]}) {
                        auto const declared{declarations_.items(soas.members[schema])};
                        for (auto const column : declarations_.items(*columns)) {
                            auto const found{std::ranges::find(declared, column, [&](auto member) {
                                return members.name[member];
                            })};
                            if (found == declared.end() ||
                                members.kind[*found] != SoaMemberKind::array) {
                                throw std::invalid_argument{
                                    context + " append column must name a direct array member: " +
                                    text(column)};
                            }
                        }
                    }
                    if (read_only && !functions.is_const[function] &&
                        !functions.is_static[function]) {
                        throw std::invalid_argument{context +
                                                    " read-only view functions must be const"};
                    }
                    if (functions.definition_in_source[function] &&
                        !declarations_.module_settings_store
                             .source[declarations_.normal_modules.settings[module]]) {
                        throw std::invalid_argument{
                            context + " out-of-line function requires a source output"};
                    }
                    if (functions.is_static[function] &&
                        std::ranges::any_of(declarations_.items(functions.body_lines[function]),
                                            [&](auto const line) {
                                                return view(line).find("$column(") !=
                                                       std::string::npos;
                                            })) {
                        throw std::invalid_argument{
                            context + " static function cannot access a receiver column"};
                    }
                    for (auto const body : declarations_.items(functions.body_lines[function])) {
                        validate_soa_body_columns(graph_, module, schema, view(body));
                    }
                    require_identifier(text(functions.name[function]),
                                       "SOA '" + schema_name + "' function name");
                    if (text(functions.name[function]) == schema_name) {
                        throw std::invalid_argument{context + " collides with its owning type"};
                    }
                    reject_generated_name_collision(text(functions.name[function]), context, false);
                }
            };
            std::set<std::string> owner_signatures;
            std::set<std::string> view_signatures;
            validate_functions(soas.functions[schema], owner_signatures, false, true);
            validate_functions(soas.const_view_functions[schema], view_signatures, true);
            validate_functions(soas.mutable_view_functions[schema], view_signatures, false);
        }
        for (auto const& root : schemas) {
            std::set<std::string> flattened_names;
            auto visit = [&](auto&& self,
                             lispb::codegen::DeclarationHandle schema,
                             lispb::codegen::ModuleHandle current_module,
                             std::string const& prefix) -> void {
                std::set<std::string> accessors;
                for (auto const member : declarations_.items(soas.members[schema])) {
                    auto const flattened{prefix.empty()
                                             ? text(members.name[member])
                                             : prefix + "_" + text(members.name[member])};
                    if (flattened == "column_count" || flattened.contains("__") ||
                        flattened.back() == '_') {
                        throw std::invalid_argument{
                            "Single-allocation leaf would generate reserved identifiers: " +
                            flattened};
                    }
                    auto const accessor{members.kind[member] == SoaMemberKind::nested
                                            ? "view_" + text(members.name[member])
                                            : text(members.name[member])};
                    if (!accessors.insert(accessor).second) {
                        throw std::invalid_argument{"Duplicate compact view accessor: " + accessor};
                    }
                    if (text(members.name[member]) == "validate" ||
                        text(members.name[member]) == "capacity_blocks") {
                        throw std::invalid_argument{"Member collides with compact view API: " +
                                                    text(members.name[member])};
                    }
                    if (members.kind[member] != SoaMemberKind::nested) {
                        if (!flattened_names.insert(flattened).second) {
                            throw std::invalid_argument{
                                "Single-allocation flattened leaf name collision: " + flattened};
                        }
                        continue;
                    }
                    auto const occurrence{graph_.resolved(current_module, members.type[member])};
                    auto const& occurrences{graph_.occurrences()};
                    auto const child{occurrences.type[occurrences.index(occurrence)]};
                    self(self, graph_.source_declaration(child), graph_.module(child), flattened);
                }
            };
            visit(visit, root, module, {});
        }
    }

    void validate_static_table(lispb::codegen::DeclarationHandle const& table) {
        auto const& tables{declarations_.static_tables};
        auto const& rows{declarations_.static_table_rows};
        auto const& columns{declarations_.static_table_columns};
        auto const& groups{declarations_.static_table_groups};
        auto const table_context{"Static table '" + text(tables.name[table]) + "'"};

        require_identifier(text(tables.name[table]), "Static table name");

        validate_export_specifier(tables.export_specifier[table],
                                  table_context + " export specifier");
        if (tables.rows[table].empty()) {
            throw std::invalid_argument{table_context + " must have rows"};
        }
        if (tables.columns[table].empty()) {
            throw std::invalid_argument{table_context + " must have columns"};
        }

        std::vector<std::string> row_names;
        std::set<std::string> generated_names{
            text(tables.name[table]), "num_rows", "num", "apply_arrays", "apply_array_pairs"};
        for (auto const row : declarations_.items(tables.rows[table])) {
            require_identifier(text(rows.name[row]), table_context + " row name");
            row_names.push_back(text(rows.name[row]));
            generated_names.insert(text(rows.name[row]) + "_index");
        }
        require_unique_names(row_names, table_context + " rows");

        std::vector<std::string> column_names;
        for (auto const column : declarations_.items(tables.columns[table])) {
            require_identifier(text(columns.name[column]), table_context + " column name");
            if (generated_names.contains(text(columns.name[column]))) {
                throw std::invalid_argument{table_context + " column '" +
                                            text(columns.name[column]) +
                                            "' collides with generated API"};
            }
            column_names.push_back(text(columns.name[column]));
            validate_type(columns.type[column],
                          table_context + " column '" + text(columns.name[column]) + "'");
        }
        require_unique_names(column_names, table_context + " columns");

        std::set<std::string> column_name_set{column_names.begin(), column_names.end()};
        std::set<std::string> member_names{generated_names};
        member_names.insert(column_names.begin(), column_names.end());
        std::set<std::string> group_names;
        for (auto const group : declarations_.items(tables.groups[table])) {
            auto const context{table_context + " group '" + text(groups.name[group]) + "'"};
            require_identifier(text(groups.name[group]), table_context + " group name");
            if (!group_names.insert(text(groups.name[group])).second) {
                throw std::invalid_argument{"Duplicate static table group name: " +
                                            text(groups.name[group])};
            }
            validate_type(groups.type[group], context + " type");
            if (groups.columns[group].empty()) {
                throw std::invalid_argument{context + " must have columns"};
            }

            for (auto const column_name : declarations_.items(groups.columns[group])) {
                require_identifier(column_name, context + " column name");
                if (!column_name_set.contains(text(column_name))) {
                    throw std::invalid_argument{context + " references unknown column '" +
                                                text(column_name) + "'"};
                }
            }
            require_unique_names(groups.columns[group], context + " columns");

            auto const getter_name{"get_" + text(groups.name[group])};
            if (!member_names.insert(getter_name).second) {
                throw std::invalid_argument{context + " getter '" + getter_name +
                                            "' collides with another table member"};
            }
        }
    }

    void validate_facade(lispb::codegen::DeclarationHandle const& facade,
                         lispb::codegen::ModuleSettingsId const& settings,
                         TypeRegistry const& types) {
        auto const& facades{declarations_.facades};
        auto const& methods{declarations_.facade_methods};
        auto const facade_context{"Facade '" + text(facades.name[facade]) + "'"};

        require_identifier(text(facades.name[facade]), "Facade name");
        require_identifier(text(facades.target_member_name[facade]),
                           facade_context + " target member");
        validate_type(facades.target_type[facade], facade_context + " target");
        if (facades.methods[facade].empty()) {
            throw std::invalid_argument{facade_context + " must have methods"};
        }
        if (facades.definitions_in_source[facade] &&
            !declarations_.module_settings_store.source[settings].has_value()) {
            throw std::invalid_argument{facade_context +
                                        " definitions in source require a source output"};
        }

        require_unique_names(facades.friends[facade], facade_context + " friends");
        for (auto const friend_name : declarations_.items(facades.friends[facade])) {
            auto name{view(friend_name)};
            if (name.starts_with("::")) {
                name.remove_prefix(2);
            }
            require_qualified_identifier(name, facade_context + " friend");
        }
        validate_export_specifier(facades.export_specifier[facade],
                                  facade_context + " export specifier");
        if (facades.reference_target[facade] && !facades.validation_lines[facade].empty()) {
            throw std::invalid_argument{facade_context +
                                        " reference targets cannot have target validation"};
        }
        if (text(facades.target_member_name[facade]) == "bind") {
            throw std::invalid_argument{facade_context + " target member collides with bind"};
        }
        for (auto const dependency : declarations_.items(facades.validation_dependencies[facade])) {
            validate_dependency(dependency, types, facade_context);
        }
        std::set<std::string> method_signatures;
        if (!facades.reference_target[facade]) {
            method_signatures.insert(signature(
                strings_.intern("bind"),
                {std::string{types.cpp_types.spelling(resolved_type(facades.target_type[facade]))} +
                 "&"},
                false));
        }
        for (auto const method : declarations_.items(facades.methods[facade])) {
            require_identifier(text(methods.name[method]), facade_context + " method name");
            if (text(methods.name[method]) == text(facades.target_member_name[facade]) ||
                text(methods.name[method]) == text(facades.name[facade])) {
                throw std::invalid_argument{facade_context + " method '" +
                                            text(methods.name[method]) +
                                            "' collides with a generated member"};
            }
            if (methods.target_name[method].has_value()) {
                require_identifier(*methods.target_name[method],
                                   facade_context + " method target name");
            }
            validate_type(methods.return_type[method],
                          facade_context + " method '" + text(methods.name[method]) + "' return");
            auto const context{facade_context + " method '" + text(methods.name[method]) + "'"};
            auto const parameter_types{
                parameter_type_names(methods.parameters[method], types, context)};
            auto const method_signature{
                signature(methods.name[method], parameter_types, methods.is_const[method])};
            if (!method_signatures.insert(method_signature).second) {
                throw std::invalid_argument{"Duplicate " + context + " signature"};
            }
        }
    }

    void validate_settings_module(lispb::codegen::ModuleHandle const& module,
                                  TypeRegistry const& types) {
        auto const& modules{declarations_.settings_modules};
        auto const& settings{declarations_.settings};
        auto const& controls{declarations_.setting_controls};
        auto const& categories{declarations_.setting_categories};

        auto const context{
            "Settings module '" +
            text(declarations_.module_settings_store.name[modules.settings[module]]) + "'"};
        if (!declarations_.module_settings_store.source[modules.settings[module]].has_value()) {
            throw std::invalid_argument{context + " must have a source output"};
        }
        require_identifier(text(modules.api_name[module]), context + " API name");
        require_identifier(text(modules.state_name[module]), context + " state name");
        validate_export_specifier(modules.export_specifier[module], context + " export specifier");
        if (modules.categories[module].empty()) {
            throw std::invalid_argument{context + " must have categories"};
        }
        if (modules.settings_list[module].empty()) {
            throw std::invalid_argument{context + " must have settings"};
        }

        std::set<std::string> category_names;
        for (auto const category : declarations_.items(modules.categories[module])) {
            require_identifier(text(categories.name[category]), context + " category name");
            require_value(text(categories.label[category]), context + " category label");
            if (!category_names.insert(text(categories.name[category])).second) {
                throw std::invalid_argument{
                    context + " has duplicate category: " + text(categories.name[category])};
            }
        }

        std::set<std::string> setting_names;
        for (auto const setting : declarations_.items(modules.settings_list[module])) {
            auto const control{settings.control[setting]};
            auto const setting_context{context + " setting '" + text(settings.name[setting]) + "'"};
            require_identifier(text(settings.name[setting]), setting_context + " name");
            if (!setting_names.insert(text(settings.name[setting])).second) {
                throw std::invalid_argument{
                    context + " has duplicate setting: " + text(settings.name[setting])};
            }
            require_value(text(settings.label[setting]), setting_context + " label");
            if (settings.tooltip[setting].has_value()) {
                require_value(*settings.tooltip[setting], setting_context + " tooltip");
            }
            if (!category_names.contains(text(settings.category[setting]))) {
                throw std::invalid_argument{
                    setting_context + " has unknown category: " + text(settings.category[setting])};
            }
            validate_type(settings.value_type[setting], setting_context + " value");
            require_identifier(text(settings.backend[setting]), setting_context + " backend");
            if (controls.options_provider[control].has_value()) {
                require_identifier(*controls.options_provider[control],
                                   setting_context + " options provider");
            }
            if (controls.availability_provider[control].has_value()) {
                require_identifier(*controls.availability_provider[control],
                                   setting_context + " availability provider");
            }

            auto const has_range{controls.minimum[control].has_value() ||
                                 controls.maximum[control].has_value() ||
                                 controls.step[control].has_value()};
            auto const is_range{controls.kind[control] == SettingControlKind::float_range ||
                                controls.kind[control] == SettingControlKind::integer_range};
            if (is_range) {
                if (!controls.minimum[control].has_value() ||
                    !controls.maximum[control].has_value() || !controls.step[control].has_value()) {
                    throw std::invalid_argument{setting_context +
                                                " range requires min, max, and step"};
                }
                if (*controls.minimum[control] >= *controls.maximum[control] ||
                    *controls.step[control] <= 0.0) {
                    throw std::invalid_argument{setting_context + " has an invalid range"};
                }
            } else if (has_range) {
                throw std::invalid_argument{setting_context +
                                            " non-range control has range values"};
            }
            if (controls.kind[control] == SettingControlKind::choice &&
                !controls.options_provider[control].has_value()) {
                throw std::invalid_argument{setting_context +
                                            " choice requires an options provider"};
            }
            if (controls.kind[control] == SettingControlKind::custom) {
                if (!controls.custom_row[control].has_value()) {
                    throw std::invalid_argument{setting_context +
                                                " custom control requires custom_row"};
                }
                require_identifier(*controls.custom_row[control], setting_context + " custom row");
            } else if (controls.custom_row[control].has_value()) {
                throw std::invalid_argument{setting_context + " non-custom control has custom_row"};
            }
            auto const type{
                std::string{types.cpp_types.spelling(resolved_type(settings.value_type[setting]))}};
            if (controls.kind[control] == SettingControlKind::toggle && type != "bool") {
                throw std::invalid_argument{setting_context + " toggle value type must be bool"};
            }
            if (controls.kind[control] == SettingControlKind::float_range && type != "float") {
                throw std::invalid_argument{setting_context +
                                            " float range value type must be float"};
            }
            if (controls.kind[control] == SettingControlKind::integer_range &&
                type != "std::int32_t") {
                throw std::invalid_argument{setting_context +
                                            " integer range value type must be std::int32_t"};
            }
        }
    }

    void validate_normal_module(lispb::codegen::ModuleHandle const& module,
                                Manifest const& manifest,
                                std::map<std::string, DeclarationHandle>& generated_names) {
        auto const& modules{declarations_.normal_modules};
        auto const& settings{declarations_.module_settings_store};

        if (modules.enum_helper_namespace[module].has_value()) {
            require_qualified_identifier(*modules.enum_helper_namespace[module],
                                         "Module helper namespace");
        }
        std::vector<lispb::codegen::DeclarationHandle> schemas;
        std::set<lispb::StringId> names;
        std::set<lispb::StringId> local_generated_names;
        auto const settings_row{settings.index(modules.settings[modules.index(module)])};
        auto const namespace_name{settings.namespace_name[settings_row]};
        auto const prefix{namespace_name ? text(*namespace_name) + "::" : std::string{}};
        for (auto const declaration : declarations_.items(modules.declarations[module])) {
            auto const name{declaration_name(declarations_, declaration)};
            if (!names.insert(name).second) {
                throw std::invalid_argument{"Duplicate declaration name in module '" +
                                            text(settings.name[modules.settings[module]]) +
                                            "': " + text(name)};
            }
            for (auto const generated_name : generated_cpp_names(declarations_, declaration)) {
                auto const relative_name{text(generated_name)};
                auto const qualified_name{prefix + relative_name};
                auto const helper{declaration.kind() != DeclarationKind::soa &&
                                  relative_name.starts_with("detail::")};
                auto namespace_collision{packed_helper_namespaces_.contains(qualified_name) &&
                                         declaration.kind() != DeclarationKind::soa};
                if (helper) {
                    auto const scope{prefix + "detail"};
                    auto const existing_scope{generated_names.find(scope)};
                    namespace_collision = namespace_collision ||
                                          (existing_scope != generated_names.end() &&
                                           existing_scope->second.kind() != DeclarationKind::soa);
                    packed_helper_namespaces_.insert(scope);
                    packed_helper_names_.insert(qualified_name);
                }
                auto const [existing,
                            inserted]{generated_names.emplace(qualified_name, declaration)};
                // Preserve ordinary cross-module ambiguity handling while reserving SoA scopes.
                auto const soa_collision{!inserted &&
                                         (declaration.kind() == DeclarationKind::soa ||
                                          existing->second.kind() == DeclarationKind::soa)};
                auto const helper_collision{
                    !inserted && packed_helper_names_.contains(qualified_name) &&
                    declaration_name(declarations_, existing->second) != name};
                if (!local_generated_names.insert(generated_name).second || soa_collision ||
                    helper_collision || namespace_collision) {
                    throw std::invalid_argument{"Generated C++ name collision in module '" +
                                                text(settings.name[modules.settings[module]]) +
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
                    case lispb::codegen::DeclarationKind::record: {

                        validate_record(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::union_type: {

                        validate_union(value);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::integer_scalar: {

                        validate_integer_scalar(value, manifest.types);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::linear_quantized: {

                        validate_linear_quantized(value);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::integer_varint: {

                        validate_integer_varint(value);
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

                        validate_optional_sentinel(value);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::optional_presence_bit: {

                        validate_optional_presence_bit(value);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::static_table: {

                        validate_static_table(value);
                        break;
                    }
                    case lispb::codegen::DeclarationKind::facade: {

                        validate_facade(value, modules.settings[module], manifest.types);
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
            auto const decls{
                declarations_.items(declarations_.normal_modules.declarations[module])};
            for (auto const declaration : decls) {
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
        auto const& settings{declarations_.module_settings_store};
        auto const& modules{declarations_.normal_modules};
        auto const& umbrellas{declarations_.umbrella_modules};
        auto const& settings_modules{declarations_.settings_modules};

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
                        auto const settings_id{modules.settings[module]};
                        validate_settings(settings_id);
                        if (!module_names.insert(settings.name[settings_id]).second) {
                            throw std::invalid_argument{"Duplicate module name: " +
                                                        text(settings.name[settings_id])};
                        }
                        for (auto const& path : {std::optional{settings.header[settings_id]},
                                                 settings.source[settings_id]}) {
                            if (path.has_value() && path->value != 0 &&
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
                        auto const settings_id{settings_modules.settings[module]};
                        validate_settings(settings_id);
                        if (!module_names.insert(settings.name[settings_id]).second) {
                            throw std::invalid_argument{"Duplicate module name: " +
                                                        text(settings.name[settings_id])};
                        }
                        for (auto const& path : {std::optional{settings.header[settings_id]},
                                                 settings.source[settings_id]}) {
                            if (path.has_value() && path->value != 0 &&
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
                        auto const settings_id{umbrellas.settings[module]};
                        validate_settings(settings_id);
                        if (!module_names.insert(settings.name[settings_id]).second) {
                            throw std::invalid_argument{"Duplicate module name: " +
                                                        text(settings.name[settings_id])};
                        }
                        for (auto const& path : {std::optional{settings.header[settings_id]},
                                                 settings.source[settings_id]}) {
                            if (path.has_value() && path->value != 0 &&
                                !output_paths
                                     .insert(output_path_key(std::filesystem::path{text(*path)}))
                                     .second) {
                                throw std::invalid_argument{"Duplicate generated output path: " +
                                                            text(*path)};
                            }
                        }

                        if (settings.source[settings_id].has_value()) {
                            throw std::invalid_argument{"Umbrella module '" +
                                                        text(settings.name[settings_id]) +
                                                        "' must not have a source output"};
                        }
                        if (settings.namespace_name[settings_id].has_value()) {
                            throw std::invalid_argument{"Umbrella module '" +
                                                        text(settings.name[settings_id]) +
                                                        "' must not have a namespace"};
                        }
                        if (umbrellas.headers[module].empty()) {
                            throw std::invalid_argument{"Umbrella module '" +
                                                        text(settings.name[settings_id]) +
                                                        "' must have headers"};
                        }
                        require_unique_names(umbrellas.headers[module],
                                             "Umbrella module '" +
                                                 text(settings.name[settings_id]) + "' headers");
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
    std::set<std::string> packed_helper_names_;
    std::set<std::string> packed_helper_namespaces_;
};

} // namespace

void validate_integer_domains(Manifest const& manifest, lispb::schema::TypeGraph const& graph) {
    Validator{manifest, graph}.run_integer_domains();
}

void validate_manifest(Manifest const& manifest, lispb::schema::TypeGraph const& graph) {
    Validator{manifest, graph}.run(manifest);
}

} // namespace lispb::codegen::detail
