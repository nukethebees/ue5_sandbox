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

void require_identifier(std::string_view value, std::string const& context) {
    require_value(value, context);
    if (!is_identifier_start(static_cast<unsigned char>(value.front())) ||
        !std::ranges::all_of(value.substr(1), [](char const character) {
            return is_identifier_character(static_cast<unsigned char>(character));
        })) {
        throw std::invalid_argument{context + " must be a C++ identifier: " +
                                    std::string{value}};
    }
}

void require_identifier_fragment(std::string_view value, std::string const& context) {
    require_value(value, context);
    if (!std::ranges::all_of(value, [](char const character) {
            return is_identifier_character(static_cast<unsigned char>(character));
        })) {
        throw std::invalid_argument{context + " must contain only C++ identifier characters: " +
                                    std::string{value}};
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
    throw std::invalid_argument{context + " must be a qualified C++ identifier: " +
                                std::string{value}};
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

void require_unique_names(std::vector<std::string> const& names,
                          std::string const& context) {
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
               std::string const& suffix) -> std::string {
    std::string result{name + "("};
    for (auto const& type : parameter_types) {
        result += type + ";";
    }
    return result + ")" + suffix;
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
        require_value(*settings.header_include,
                      "Module '" + settings.name + "' header include");
    }
    if (settings.namespace_name.has_value()) {
        require_qualified_identifier(*settings.namespace_name,
                                     "Module '" + settings.name + "' namespace");
    }
    require_unique_names(settings.include_order,
                         "Module '" + settings.name + "' include order");
}

void validate_soa(SoaModuleSchema const& module,
                  std::map<std::string, CppType> const& types) {
    if (module.structs.empty()) {
        throw std::invalid_argument{"SOA module '" + module.settings.name +
                                    "' must have schemas"};
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
            require_identifier(member.name,
                               "SOA '" + schema.name + "' member name");
            validate_type(member.type, types, "SOA '" + schema.name + "' member '" +
                                                  member.name + "'");
            if (member.kind == SoaMemberKind::array && member.fixed_schema.has_value()) {
                throw std::invalid_argument{"Array member '" + member.name +
                                            "' must not specify fixed_schema"};
            }
        }
        require_unique_names(member_names, "SOA '" + schema.name + "' members");
        require_unique_operations(schema.operations, "SOA '" + schema.name + "' operations");
        if (schema.fixed.has_value()) {
            require_identifier(schema.fixed->storage_name,
                               "SOA '" + schema.name + "' fixed storage name");
            require_unique_names(schema.fixed->containers,
                                 "SOA '" + schema.name + "' fixed containers");
            add_generated_type(schema.fixed->storage_name);
            for (auto const& container : schema.fixed->containers) {
                require_identifier(container,
                                   "SOA '" + schema.name + "' fixed container name");
                add_generated_type(container);
            }
        }
        if (schema.equivalent_type.has_value()) {
            validate_type(*schema.equivalent_type, types,
                          "SOA '" + schema.name + "' equivalent");
        }
        std::set<std::string> function_signatures;
        for (auto const& function : schema.functions) {
            auto const context{"SOA '" + schema.name + "' function '" + function.name + "'"};
            require_value(function.name, "SOA '" + schema.name + "' function name");
            validate_type(function.return_type, types, context + " return");
            auto const parameter_types{parameter_type_names(function.parameters, types, context)};
            auto const function_signature{
                signature(function.name, parameter_types, function.suffix)};
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
            auto const found{std::find_if(module.structs.begin(),
                                         module.structs.end(),
                                         [&](SoaSchema const& candidate) {
                                             return candidate.name == *member.fixed_schema;
                                         })};
            if (found == module.structs.end() || !found->fixed.has_value()) {
                throw std::invalid_argument{"Unknown fixed nested schema: " +
                                            *member.fixed_schema};
            }
        }
    }
    if (!module.settings.source.has_value()) {
        throw std::invalid_argument{"SOA module '" + module.settings.name +
                                    "' must have a source output"};
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
            require_identifier(component,
                               "Homogeneous layout '" + layout.name + "' component");
        }
        if (layout.value_types.empty()) {
            throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                        "' must have value types"};
        }
        auto has_equivalent{layout.value_types.front().equivalent_type.has_value()};
        std::vector<std::string> suffixes;
        std::set<std::string> equivalent_specialisations;
        for (auto const& value : layout.value_types) {
            require_identifier_fragment(
                value.suffix, "Homogeneous layout '" + layout.name + "' value suffix");
            suffixes.push_back(value.suffix);
            auto const storage_name{"F" + layout.name + value.suffix};
            if (!storage_names.insert(storage_name).second) {
                throw std::invalid_argument{"Duplicate homogeneous storage name: " +
                                            storage_name};
            }
            validate_type(value.type, types, "Homogeneous layout '" + layout.name + "'");
            if (value.equivalent_type.has_value() != has_equivalent) {
                throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                            "' must define all equivalent types or none"};
            }
            if (value.equivalent_type.has_value()) {
                validate_type(*value.equivalent_type, types,
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
            if (!value.input_types.empty()) {
                std::set<char> parameter_names;
                for (auto const& component : layout.components) {
                    if (!parameter_names.insert(component.front()).second) {
                        throw std::invalid_argument{
                            "Homogeneous layout '" + layout.name +
                            "' input components must have unique initials"};
                    }
                }
            }
            std::set<std::string> input_types;
            for (auto const& input : value.input_types) {
                validate_type(input, types,
                              "Homogeneous layout '" + layout.name + "' input");
                auto const input_spelling{resolve_type(input, types).spelling};
                if (!input_types.insert(input_spelling).second) {
                    throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                                "' has duplicate input type: " +
                                                input_spelling};
                }
            }
        }
        require_unique_names(suffixes,
                             "Homogeneous layout '" + layout.name + "' value suffixes");
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
        require_identifier(component,
                           "Vector module '" + module.settings.name + "' component");
        if (!parameter_names.insert(component.front()).second) {
            throw std::invalid_argument{"Vector module '" + module.settings.name +
                                        "' components must have unique initials"};
        }
    }
    validate_type(module.value_type, types, "Vector module '" + module.settings.name + "' value");
    validate_type(module.equivalent_type, types,
                  "Vector module '" + module.settings.name + "' equivalent");
    if (!module.settings.source.has_value()) {
        throw std::invalid_argument{"Vector module '" + module.settings.name +
                                    "' must have a source output"};
    }
}

void validate_facade(FacadeModuleSchema const& module,
                     std::map<std::string, CppType> const& types) {
    auto const& facade{module.facade};
    require_identifier(facade.name, "Facade name");
    require_identifier(facade.target_member_name,
                       "Facade '" + facade.name + "' target member");
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
    for (auto const& dependency : facade.validation_dependencies) {
        validate_dependency(dependency, types, "Facade '" + facade.name + "'");
    }
    std::set<std::string> method_signatures{
        signature("bind", {resolve_type(facade.target_type, types).spelling + "&"}, {})};
    for (auto const& method : facade.methods) {
        require_identifier(method.name, "Facade '" + facade.name + "' method name");
        if (method.target_name.has_value()) {
            require_identifier(*method.target_name,
                               "Facade '" + facade.name + "' method target name");
        }
        validate_type(method.return_type, types,
                      "Facade '" + facade.name + "' method '" + method.name + "' return");
        auto const context{"Facade '" + facade.name + "' method '" + method.name + "'"};
        auto const parameter_types{parameter_type_names(method.parameters, types, context)};
        auto const method_signature{signature(method.name, parameter_types, method.suffix)};
        if (!method_signatures.insert(method_signature).second) {
            throw std::invalid_argument{"Duplicate " + context + " signature"};
        }
    }
}

} // namespace

void validate_manifest(Manifest const& manifest) {
    if (manifest.schema_version != 1) {
        throw std::invalid_argument{"Unsupported manifest schema version: " +
                                    std::to_string(manifest.schema_version)};
    }
    std::set<std::string> module_names;
    std::set<std::string> output_paths;
    for (auto const& [name, type] : manifest.types) {
        require_value(name, "Type name");
        require_value(type.spelling, "Type '" + name + "' spelling");
    }
    for (auto const& schema : manifest.modules) {
        std::visit(
            [&](auto const& module) {
                validate_settings(module.settings);
                if (!module_names.insert(module.settings.name).second) {
                    throw std::invalid_argument{"Duplicate module name: " +
                                                module.settings.name};
                }
                for (auto const& path :
                     {std::optional{module.settings.header}, module.settings.source}) {
                    if (path.has_value() &&
                        !output_paths.insert(output_path_key(*path)).second) {
                        throw std::invalid_argument{"Duplicate generated output path: " +
                                                    path->string()};
                    }
                }
                using T = std::decay_t<decltype(module)>;
                if constexpr (std::is_same_v<T, SoaModuleSchema>) {
                    validate_soa(module, manifest.types);
                } else if constexpr (std::is_same_v<T, HomogeneousModuleSchema>) {
                    validate_homogeneous(module, manifest.types);
                } else if constexpr (std::is_same_v<T, VectorModuleSchema>) {
                    validate_vector(module, manifest.types);
                } else if constexpr (std::is_same_v<T, FacadeModuleSchema>) {
                    validate_facade(module, manifest.types);
                } else if constexpr (std::is_same_v<T, UmbrellaModuleSchema>) {
                    if (module.settings.source.has_value()) {
                        throw std::invalid_argument{"Umbrella module '" +
                                                    module.settings.name +
                                                    "' must not have a source output"};
                    }
                    if (module.settings.namespace_name.has_value()) {
                        throw std::invalid_argument{"Umbrella module '" +
                                                    module.settings.name +
                                                    "' must not have a namespace"};
                    }
                    require_unique_names(module.headers,
                                         "Umbrella module '" + module.settings.name + "' headers");
                }
            },
            schema);
    }
}

} // namespace codegen::detail
