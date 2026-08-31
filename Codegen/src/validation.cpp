#include "validation.h"
#include "lowering_utils.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace codegen::detail {
namespace {

void require_value(std::string_view value, std::string const& context) {
    if (value.empty()) {
        throw std::invalid_argument{context + " must not be empty"};
    }
}

auto is_identifier_start(unsigned char const character) -> bool {
    return std::isalpha(character) != 0 || character == '_';
}

auto is_identifier_character(unsigned char const character) -> bool {
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
        "apply_array_pairs",
        "apply_arrays",
        "apply_permutation",
        "at",
        "copy_element",
        "copy_elements",
        "copy_to_tail",
        "get_const_view",
        "get_view",
        "is_empty",
        "left",
        "num",
        "right",
        "slice",
        "sort",
        "validate_array_sizes",
    };
    return names.contains(value);
}

auto is_generated_mutation_name(std::string_view const value) -> bool {
    static std::set<std::string_view> const names{
        "add",
        "add_defaulted",
        "add_uninitialised",
        "add_zeroed",
        "append_from",
        "empty",
        "remove_at_swap",
        "reserve",
        "reset",
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
        throw std::invalid_argument{context + " must be a C++ identifier: " + std::string{value}};
    }
    if (is_cpp_keyword(value) || is_reserved_identifier(value)) {
        throw std::invalid_argument{
            context + " must not be a reserved C++ identifier: " + std::string{value}};
    }
}

void require_identifier_fragment(std::string_view value, std::string const& context) {
    require_value(value, context);
    if (!std::ranges::all_of(value, [](char const character) {
            return is_identifier_character(static_cast<unsigned char>(character));
        })) {
        throw std::invalid_argument{
            context + " must contain only C++ identifier characters: " + std::string{value}};
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

void require_unique_operations(std::vector<StorageOperation> const& operations,
                               std::string const& context) {
    std::set<StorageOperation> unique;
    for (auto const operation : operations) {
        if (!unique.insert(operation).second) {
            throw std::invalid_argument{context + " contains duplicate value: " +
                                        std::to_string(static_cast<int>(operation))};
        }
    }
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

void validate_type(TypeRef const& type,
                   std::map<std::string, CppType> const& types,
                   std::string const& context) {
    require_value(type.name, context + " type");
    if (type.nested.has_value()) {
        require_value(*type.nested, context + " nested type");
    }
    static_cast<void>(resolve_type(type, types));
}

void validate_type_dependency(TypeDependency const& dependency, std::string const& context) {
    require_value(dependency.spelling, context + " spelling");
    if (dependency.header.has_value()) {
        require_value(*dependency.header, context + " header");
    }
    for (auto const& nested : dependency.dependencies) {
        validate_type_dependency(nested, context + " nested dependency");
    }
}

void validate_export_specifier(std::optional<std::string> const& value,
                               std::string const& context) {
    if (value.has_value()) {
        require_identifier(*value, context);
    }
}

void validate_dependency(std::string const& key,
                         std::map<std::string, CppType> const& types,
                         std::string const& context) {
    require_value(key, context + " dependency");
    auto const found{types.find(key)};
    if (found == types.end() || found->second.dependencies.empty()) {
        throw std::invalid_argument{context + " has unknown dependency: " + key};
    }
}

auto parameter_type_names(std::vector<ParameterSchema> const& parameters,
                          std::map<std::string, CppType> const& types,
                          std::string const& context) -> std::vector<std::string> {
    std::vector<std::string> names;
    std::vector<std::string> type_names;
    bool has_default{};
    for (auto const& parameter : parameters) {
        names.push_back(parameter.name);
        require_identifier(parameter.name, context + " parameter name");
        validate_type(parameter.type, types, context + " parameter '" + parameter.name + "'");
        type_names.push_back(resolve_type(parameter.type, types).spelling);
        if (parameter.default_value.has_value()) {
            has_default = true;
        } else if (has_default) {
            throw std::invalid_argument{context +
                                        " has a non-defaulted parameter after a defaulted one"};
        }
    }
    require_unique_names(names, context + " parameters");
    return type_names;
}

auto signature(std::string const& name,
               std::vector<std::string> const& parameter_types,
               bool const is_const) -> std::string {
    std::string result{name + "("};
    for (auto const& type : parameter_types) {
        result += type + ";";
    }
    return result + ")" + (is_const ? " const" : "");
}

void validate_settings(ModuleSettings const& settings) {
    require_value(settings.name, "Module name");
    if (settings.header.empty()) {
        throw std::invalid_argument{"Module '" + settings.name + "' header must not be empty"};
    }
    if (settings.source.has_value() && settings.source->empty()) {
        throw std::invalid_argument{"Module '" + settings.name + "' source must not be empty"};
    }
    if (settings.header_include.has_value()) {
        require_value(*settings.header_include, "Module '" + settings.name + "' header include");
    }
    if (settings.namespace_name.has_value()) {
        require_qualified_identifier(*settings.namespace_name,
                                     "Module '" + settings.name + "' namespace");
    }
    require_unique_names(settings.include_order, "Module '" + settings.name + "' include order");
}

void validate_enum(EnumModuleSchema const& module,
                   std::map<std::string, CppType> const& types) {
    if (module.enums.empty()) {
        throw std::invalid_argument{"Enum module '" + module.settings.name +
                                    "' must have enums"};
    }
    if (module.helper_namespace.has_value()) {
        require_qualified_identifier(*module.helper_namespace,
                                     "Enum module '" + module.settings.name +
                                         "' helper namespace");
    }
    std::set<std::string> enum_names;
    for (auto const& schema : module.enums) {
        require_identifier(schema.name, "Enum name");
        if (!enum_names.insert(schema.name).second) {
            throw std::invalid_argument{"Duplicate enum name: " + schema.name};
        }
        validate_type(schema.underlying_type, types, "Enum '" + schema.name + "' underlying");
        validate_export_specifier(schema.export_specifier,
                                  "Enum '" + schema.name + "' export specifier");
        if (schema.reflection != EnumReflection::none &&
            module.settings.namespace_name.has_value()) {
            throw std::invalid_argument{"Reflected enum '" + schema.name +
                                        "' must be declared at global scope"};
        }
        if (schema.values.empty()) {
            throw std::invalid_argument{"Enum '" + schema.name + "' must have values"};
        }
        std::vector<std::string> value_names;
        for (auto const& value : schema.values) {
            require_identifier(value.name, "Enum '" + schema.name + "' value name");
            value_names.push_back(value.name);
            if (value.initializer.has_value()) {
                require_value(*value.initializer,
                              "Enum '" + schema.name + "' value '" + value.name +
                                  "' initializer");
            }
            if (value.display_name.has_value()) {
                require_value(*value.display_name,
                              "Enum '" + schema.name + "' value '" + value.name +
                                  "' display name");
            }
            if (value.hidden && schema.reflection == EnumReflection::none) {
                throw std::invalid_argument{"Plain enum '" + schema.name + "' value '" +
                                            value.name + "' cannot be hidden"};
            }
        }
        require_unique_names(value_names, "Enum '" + schema.name + "' values");

        if (schema.enum_array) {
            for (auto const& value : schema.values) {
                if (value.initializer.has_value()) {
                    throw std::invalid_argument{"Enum-array enum '" + schema.name +
                                                "' value '" + value.name +
                                                "' must not have an explicit initializer"};
                }
                if (value.hidden) {
                    throw std::invalid_argument{"Enum-array enum '" + schema.name +
                                                "' value '" + value.name +
                                                "' must not be hidden"};
                }
            }
        }

        std::set<EnumConversion> conversions;
        for (auto const conversion : schema.conversions) {
            if (!conversions.insert(conversion).second) {
                throw std::invalid_argument{"Enum '" + schema.name +
                                            "' contains duplicate conversion"};
            }
        }
        if (!schema.conversions.empty() && !module.settings.source.has_value()) {
            throw std::invalid_argument{"Enum '" + schema.name +
                                        "' conversions require a source output"};
        }
    }
}

void validate_soa(SoaModuleSchema const& module, std::map<std::string, CppType> const& types) {
    if (module.structs.empty()) {
        throw std::invalid_argument{"SOA module '" + module.settings.name + "' must have schemas"};
    }
    std::set<std::string> schema_names;
    std::set<std::string> generated_type_names;
    auto add_generated_type = [&](std::string const& name) {
        if (!generated_type_names.insert(name).second) {
            throw std::invalid_argument{"Duplicate generated SOA type name: " + name};
        }
    };
    for (auto const& schema : module.structs) {
        require_identifier(schema.name, "SOA name");
        if (schema.view_name.has_value()) {
            require_identifier(*schema.view_name, "SOA '" + schema.name + "' view name");
        }
        if (schema.const_view_name.has_value()) {
            require_identifier(*schema.const_view_name,
                               "SOA '" + schema.name + "' const view name");
        }
        if (!schema_names.insert(schema.name).second) {
            throw std::invalid_argument{"Duplicate SOA schema name: " + schema.name};
        }
        add_generated_type(schema.name);
        add_generated_type(schema.view_name.value_or(schema.name + "View"));
        add_generated_type(schema.const_view_name.value_or(schema.name + "ConstView"));
        if (schema.members.empty()) {
            throw std::invalid_argument{"SOA '" + schema.name + "' must have members"};
        }
        std::vector<std::string> member_names;
        for (auto const& member : schema.members) {
            member_names.push_back(member.name);
            require_identifier(member.name, "SOA '" + schema.name + "' member name");
            reject_generated_name_collision(
                member.name, "SOA '" + schema.name + "' member name", true);
            validate_type(
                member.type, types, "SOA '" + schema.name + "' member '" + member.name + "'");
            if (member.kind == SoaMemberKind::array && member.fixed_schema.has_value()) {
                throw std::invalid_argument{"Array member '" + member.name +
                                            "' must not specify fixed_schema"};
            }
        }
        require_unique_names(member_names, "SOA '" + schema.name + "' members");
        require_unique_operations(schema.operations, "SOA '" + schema.name + "' operations");
        validate_export_specifier(schema.export_specifier,
                                  "SOA '" + schema.name + "' export specifier");
        if (schema.fixed.has_value()) {
            require_identifier(schema.fixed->storage_name,
                               "SOA '" + schema.name + "' fixed storage name");
            require_unique_names(schema.fixed->containers,
                                 "SOA '" + schema.name + "' fixed containers");
            add_generated_type(schema.fixed->storage_name);
            for (auto const& container : schema.fixed->containers) {
                require_identifier(container, "SOA '" + schema.name + "' fixed container name");
                add_generated_type(container);
            }
        }
        if (schema.equivalent_type.has_value()) {
            validate_type(*schema.equivalent_type, types, "SOA '" + schema.name + "' equivalent");
        }
        std::set<std::string> function_signatures;
        for (auto const& function : schema.functions) {
            auto const context{"SOA '" + schema.name + "' function '" + function.name + "'"};
            require_identifier(function.name, "SOA '" + schema.name + "' function name");
            if (function.name == schema.name) {
                throw std::invalid_argument{context + " collides with its owning type"};
            }
            reject_generated_name_collision(function.name, context, false);
            if (function.is_inline && function.definition_in_source) {
                throw std::invalid_argument{context +
                                            " must not be both inline and defined in the source"};
            }
            if (function.template_parameters.has_value()) {
                require_value(*function.template_parameters, context + " template parameters");
            }
            if (function.requires_clause.has_value()) {
                require_value(*function.requires_clause, context + " requires clause");
            }
            validate_type(function.return_type, types, context + " return");
            if (function.trailing_return_type.has_value()) {
                validate_type(*function.trailing_return_type, types, context + " trailing return");
            }
            auto const parameter_types{parameter_type_names(function.parameters, types, context)};
            auto const function_signature{
                signature(function.name, parameter_types, function.is_const)};
            if (!function_signatures.insert(function_signature).second) {
                throw std::invalid_argument{"Duplicate " + context + " signature"};
            }
            for (auto const& dependency : function.dependencies) {
                validate_dependency(dependency, types, context);
            }
        }
    }
    for (auto const& schema : module.structs) {
        if (!schema.fixed.has_value()) {
            continue;
        }
        for (auto const& member : schema.members) {
            if (member.kind != SoaMemberKind::nested) {
                continue;
            }
            if (!member.fixed_schema.has_value()) {
                throw std::invalid_argument{"Fixed SOA '" + schema.name + "' nested member '" +
                                            member.name + "' has no fixed_schema"};
            }
            auto const found{std::find_if(
                module.structs.begin(), module.structs.end(), [&](SoaSchema const& candidate) {
                    return candidate.name == *member.fixed_schema;
                })};
            if (found == module.structs.end() || !found->fixed.has_value()) {
                throw std::invalid_argument{"Unknown fixed nested schema: " + *member.fixed_schema};
            }
        }
    }
    if (!module.settings.source.has_value()) {
        throw std::invalid_argument{"SOA module '" + module.settings.name +
                                    "' must have a source output"};
    }
}

void validate_static_table(StaticTableModuleSchema const& module,
                           std::map<std::string, CppType> const& types) {
    if (module.settings.source.has_value()) {
        throw std::invalid_argument{"Static table module '" + module.settings.name +
                                    "' must not have a source output"};
    }
    if (module.tables.empty()) {
        throw std::invalid_argument{"Static table module '" + module.settings.name +
                                    "' must have tables"};
    }

    std::set<std::string> table_names;
    for (auto const& table : module.tables) {
        require_identifier(table.name, "Static table name");
        if (!table_names.insert(table.name).second) {
            throw std::invalid_argument{"Duplicate static table name: " + table.name};
        }
        validate_export_specifier(table.export_specifier,
                                  "Static table '" + table.name + "' export specifier");
        if (table.rows.empty()) {
            throw std::invalid_argument{"Static table '" + table.name + "' must have rows"};
        }
        if (table.columns.empty()) {
            throw std::invalid_argument{"Static table '" + table.name + "' must have columns"};
        }

        std::vector<std::string> row_names;
        std::set<std::string> generated_names{
            table.name, "num_rows", "num", "apply_arrays", "apply_array_pairs"};
        for (auto const& row : table.rows) {
            require_identifier(row.name, "Static table '" + table.name + "' row name");
            row_names.push_back(row.name);
            generated_names.insert(row.name + "_index");
        }
        require_unique_names(row_names, "Static table '" + table.name + "' rows");

        std::vector<std::string> column_names;
        for (auto const& column : table.columns) {
            require_identifier(column.name,
                               "Static table '" + table.name + "' column name");
            if (generated_names.contains(column.name)) {
                throw std::invalid_argument{"Static table '" + table.name + "' column '" +
                                            column.name + "' collides with generated API"};
            }
            column_names.push_back(column.name);
            validate_type(column.type,
                          types,
                          "Static table '" + table.name + "' column '" + column.name + "'");
        }
        require_unique_names(column_names, "Static table '" + table.name + "' columns");

        std::set<std::string> column_name_set{column_names.begin(), column_names.end()};
        std::set<std::string> member_names{generated_names};
        member_names.insert(column_names.begin(), column_names.end());
        std::set<std::string> group_names;
        for (auto const& group : table.groups) {
            auto const context{"Static table '" + table.name + "' group '" + group.name + "'"};
            require_identifier(group.name, "Static table '" + table.name + "' group name");
            if (!group_names.insert(group.name).second) {
                throw std::invalid_argument{"Duplicate static table group name: " + group.name};
            }
            validate_type(group.type, types, context + " type");
            if (group.columns.empty()) {
                throw std::invalid_argument{context + " must have columns"};
            }

            for (auto const& column_name : group.columns) {
                require_identifier(column_name, context + " column name");
                if (!column_name_set.contains(column_name)) {
                    throw std::invalid_argument{context + " references unknown column '" +
                                                column_name + "'"};
                }
            }
            require_unique_names(group.columns, context + " columns");

            auto const getter_name{"get_" + group.name};
            if (!member_names.insert(getter_name).second) {
                throw std::invalid_argument{context + " getter '" + getter_name +
                                            "' collides with another table member"};
            }
        }
    }
}

void validate_homogeneous(HomogeneousModuleSchema const& module,
                          std::map<std::string, CppType> const& types) {
    if (!module.settings.source.has_value()) {
        throw std::invalid_argument{"Homogeneous module '" + module.settings.name +
                                    "' must have a source output"};
    }
    if (module.layouts.empty()) {
        throw std::invalid_argument{"Homogeneous module '" + module.settings.name +
                                    "' must have layouts"};
    }
    std::set<std::string> layout_names;
    std::set<std::string> storage_names;
    for (auto const& layout : module.layouts) {
        require_identifier_fragment(layout.name, "Homogeneous layout name");
        if (!layout_names.insert(layout.name).second) {
            throw std::invalid_argument{"Duplicate homogeneous layout name: " + layout.name};
        }
        if (layout.components.empty()) {
            throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                        "' must have components"};
        }
        require_unique_names(layout.components,
                             "Homogeneous layout '" + layout.name + "' components");
        for (auto const& component : layout.components) {
            require_identifier(component, "Homogeneous layout '" + layout.name + "' component");
            reject_generated_name_collision(
                component, "Homogeneous layout '" + layout.name + "' component", true);
        }
        std::set<char> parameter_names;
        for (auto const& component : layout.components) {
            if (!parameter_names.insert(component.front()).second) {
                throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                            "' components must have unique initials"};
            }
        }
        if (layout.value_types.empty()) {
            throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                        "' must have value types"};
        }
        auto has_equivalent{layout.value_types.front().equivalent_type.has_value()};
        std::vector<std::string> suffixes;
        std::set<std::string> equivalent_specialisations;
        for (auto const& value : layout.value_types) {
            require_identifier_fragment(value.suffix,
                                        "Homogeneous layout '" + layout.name + "' value suffix");
            suffixes.push_back(value.suffix);
            auto const storage_name{"F" + layout.name + value.suffix};
            if (!storage_names.insert(storage_name).second) {
                throw std::invalid_argument{"Duplicate homogeneous storage name: " + storage_name};
            }
            validate_type(value.type, types, "Homogeneous layout '" + layout.name + "'");
            if (value.equivalent_type.has_value() != has_equivalent) {
                throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                            "' must define all equivalent types or none"};
            }
            if (value.equivalent_type.has_value()) {
                validate_type(*value.equivalent_type,
                              types,
                              "Homogeneous layout '" + layout.name + "' equivalent");
                auto const value_spelling{resolve_type(value.type, types).spelling};
                if (!equivalent_specialisations.insert(value_spelling).second) {
                    throw std::invalid_argument{
                        "Homogeneous layout '" + layout.name +
                        "' has duplicate equivalent specialisation: " + value_spelling};
                }
            }
            if (!value.input_types.empty() && layout.components.size() > 3) {
                throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                            "' input overloads require at most three components"};
            }
            std::set<std::string> input_types;
            for (auto const& input : value.input_types) {
                validate_type(input, types, "Homogeneous layout '" + layout.name + "' input");
                auto const input_spelling{resolve_type(input, types).spelling};
                if (!input_types.insert(input_spelling).second) {
                    throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                                "' has duplicate input type: " + input_spelling};
                }
            }
        }
        require_unique_names(suffixes, "Homogeneous layout '" + layout.name + "' value suffixes");
        validate_export_specifier(layout.export_specifier,
                                  "Homogeneous layout '" + layout.name + "' export specifier");
    }
}

void validate_vector(VectorModuleSchema const& module,
                     std::map<std::string, CppType> const& types) {
    require_identifier(module.storage_name, "Vector storage name");
    if (module.components.empty() || module.components.size() > 3) {
        throw std::invalid_argument{"Vector module '" + module.settings.name +
                                    "' must have between one and three components"};
    }
    require_unique_names(module.components,
                         "Vector module '" + module.settings.name + "' components");
    std::set<char> parameter_names;
    for (auto const& component : module.components) {
        require_identifier(component, "Vector module '" + module.settings.name + "' component");
        reject_generated_name_collision(
            component, "Vector module '" + module.settings.name + "' component", true);
        if (!parameter_names.insert(component.front()).second) {
            throw std::invalid_argument{"Vector module '" + module.settings.name +
                                        "' components must have unique initials"};
        }
    }
    validate_type(module.value_type, types, "Vector module '" + module.settings.name + "' value");
    validate_type(
        module.equivalent_type, types, "Vector module '" + module.settings.name + "' equivalent");
    validate_export_specifier(module.export_specifier,
                              "Vector module '" + module.settings.name + "' export specifier");
    if (!module.settings.source.has_value()) {
        throw std::invalid_argument{"Vector module '" + module.settings.name +
                                    "' must have a source output"};
    }
}

void validate_facade(FacadeModuleSchema const& module,
                     std::map<std::string, CppType> const& types) {
    auto const& facade{module.facade};
    require_identifier(facade.name, "Facade name");
    require_identifier(facade.target_member_name, "Facade '" + facade.name + "' target member");
    validate_type(facade.target_type, types, "Facade '" + facade.name + "' target");
    if (facade.methods.empty()) {
        throw std::invalid_argument{"Facade '" + facade.name + "' must have methods"};
    }
    if (facade.definitions_in_source != module.settings.source.has_value()) {
        throw std::invalid_argument{"Facade '" + facade.name +
                                    "' source output must match definitions_in_source"};
    }
    for (auto const* access : {&facade.bind_access, &facade.method_access}) {
        if (*access != "public" && *access != "private") {
            throw std::invalid_argument{"Facade '" + facade.name +
                                        "' access must be public or private"};
        }
    }
    require_unique_names(facade.friends, "Facade '" + facade.name + "' friends");
    for (auto const& friend_name : facade.friends) {
        require_qualified_identifier(friend_name, "Facade '" + facade.name + "' friend");
    }
    validate_export_specifier(facade.export_specifier,
                              "Facade '" + facade.name + "' export specifier");
    if (facade.target_member_name == "bind") {
        throw std::invalid_argument{"Facade '" + facade.name +
                                    "' target member collides with bind"};
    }
    for (auto const& dependency : facade.validation_dependencies) {
        validate_dependency(dependency, types, "Facade '" + facade.name + "'");
    }
    std::set<std::string> method_signatures{
        signature("bind", {resolve_type(facade.target_type, types).spelling + "&"}, false)};
    for (auto const& method : facade.methods) {
        require_identifier(method.name, "Facade '" + facade.name + "' method name");
        if (method.name == facade.target_member_name || method.name == facade.name) {
            throw std::invalid_argument{"Facade '" + facade.name + "' method '" + method.name +
                                        "' collides with a generated member"};
        }
        if (method.target_name.has_value()) {
            require_identifier(*method.target_name,
                               "Facade '" + facade.name + "' method target name");
        }
        validate_type(method.return_type,
                      types,
                      "Facade '" + facade.name + "' method '" + method.name + "' return");
        auto const context{"Facade '" + facade.name + "' method '" + method.name + "'"};
        auto const parameter_types{parameter_type_names(method.parameters, types, context)};
        auto const method_signature{signature(method.name, parameter_types, method.is_const)};
        if (!method_signatures.insert(method_signature).second) {
            throw std::invalid_argument{"Duplicate " + context + " signature"};
        }
    }
}

} // namespace

void validate_manifest(Manifest const& manifest) {
    if (manifest.schema_version != manifest_schema_version) {
        throw std::invalid_argument{
            "Unsupported manifest schema version: " + std::to_string(manifest.schema_version) +
            "; expected " + std::to_string(manifest_schema_version)};
    }
    std::set<std::string> module_names;
    std::set<std::string> output_paths;
    if (manifest.modules.empty()) {
        throw std::invalid_argument{"Manifest must contain at least one module"};
    }
    for (auto const& [name, type] : manifest.types) {
        require_value(name, "Type name");
        require_value(type.spelling, "Type '" + name + "' spelling");
        for (auto const& dependency : type.dependencies) {
            validate_type_dependency(dependency, "Type '" + name + "' dependency");
        }
        for (auto const& [operation, spelling] : type.member_operations) {
            static_cast<void>(operation);
            require_identifier(spelling, "Type '" + name + "' member operation");
        }
    }
    for (auto const& schema : manifest.modules) {
        std::visit(
            [&](auto const& module) {
                validate_settings(module.settings);
                if (!module_names.insert(module.settings.name).second) {
                    throw std::invalid_argument{"Duplicate module name: " + module.settings.name};
                }
                for (auto const& path :
                     {std::optional{module.settings.header}, module.settings.source}) {
                    if (path.has_value() && !output_paths.insert(output_path_key(*path)).second) {
                        throw std::invalid_argument{"Duplicate generated output path: " +
                                                    path->string()};
                    }
                }
                using T = std::decay_t<decltype(module)>;
                if constexpr (std::is_same_v<T, EnumModuleSchema>) {
                    validate_enum(module, manifest.types);
                } else if constexpr (std::is_same_v<T, SoaModuleSchema>) {
                    validate_soa(module, manifest.types);
                } else if constexpr (std::is_same_v<T, StaticTableModuleSchema>) {
                    validate_static_table(module, manifest.types);
                } else if constexpr (std::is_same_v<T, HomogeneousModuleSchema>) {
                    validate_homogeneous(module, manifest.types);
                } else if constexpr (std::is_same_v<T, VectorModuleSchema>) {
                    validate_vector(module, manifest.types);
                } else if constexpr (std::is_same_v<T, FacadeModuleSchema>) {
                    validate_facade(module, manifest.types);
                } else if constexpr (std::is_same_v<T, UmbrellaModuleSchema>) {
                    if (module.settings.source.has_value()) {
                        throw std::invalid_argument{"Umbrella module '" + module.settings.name +
                                                    "' must not have a source output"};
                    }
                    if (module.settings.namespace_name.has_value()) {
                        throw std::invalid_argument{"Umbrella module '" + module.settings.name +
                                                    "' must not have a namespace"};
                    }
                    if (module.headers.empty()) {
                        throw std::invalid_argument{"Umbrella module '" + module.settings.name +
                                                    "' must have headers"};
                    }
                    require_unique_names(module.headers,
                                         "Umbrella module '" + module.settings.name + "' headers");
                }
            },
            schema);
    }
}

} // namespace codegen::detail
