#include "validation.h"

#include <algorithm>
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

void validate_settings(ModuleSettings const& settings) {
    require_value(settings.name, "Module name");
    if (settings.header.empty()) {
        throw std::invalid_argument{"Module '" + settings.name + "' header must not be empty"};
    }
    if (settings.source.has_value() && settings.source->empty()) {
        throw std::invalid_argument{"Module '" + settings.name + "' source must not be empty"};
    }
}

void validate_soa(SoaModuleSchema const& module,
                  std::map<std::string, CppType> const& types) {
    if (module.structs.empty()) {
        throw std::invalid_argument{"SOA module '" + module.settings.name +
                                    "' must have schemas"};
    }
    std::set<std::string> schema_names;
    for (auto const& schema : module.structs) {
        require_value(schema.name, "SOA name");
        if (!schema_names.insert(schema.name).second) {
            throw std::invalid_argument{"Duplicate SOA schema name: " + schema.name};
        }
        if (schema.members.empty()) {
            throw std::invalid_argument{"SOA '" + schema.name + "' must have members"};
        }
        std::vector<std::string> member_names;
        for (auto const& member : schema.members) {
            member_names.push_back(member.name);
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
            require_value(schema.fixed->storage_name,
                          "SOA '" + schema.name + "' fixed storage name");
            require_unique_names(schema.fixed->containers,
                                 "SOA '" + schema.name + "' fixed containers");
        }
        if (schema.equivalent_type.has_value()) {
            validate_type(*schema.equivalent_type, types,
                          "SOA '" + schema.name + "' equivalent");
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
}

void validate_homogeneous(HomogeneousModuleSchema const& module,
                          std::map<std::string, CppType> const& types) {
    if (module.layouts.empty()) {
        throw std::invalid_argument{"Homogeneous module '" + module.settings.name +
                                    "' must have layouts"};
    }
    for (auto const& layout : module.layouts) {
        require_value(layout.name, "Homogeneous layout name");
        if (layout.components.empty()) {
            throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                        "' must have components"};
        }
        require_unique_names(layout.components,
                             "Homogeneous layout '" + layout.name + "' components");
        if (layout.value_types.empty()) {
            throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                        "' must have value types"};
        }
        auto has_equivalent{layout.value_types.front().equivalent_type.has_value()};
        for (auto const& value : layout.value_types) {
            require_value(value.suffix,
                          "Homogeneous layout '" + layout.name + "' value suffix");
            validate_type(value.type, types, "Homogeneous layout '" + layout.name + "'");
            if (value.equivalent_type.has_value() != has_equivalent) {
                throw std::invalid_argument{"Homogeneous layout '" + layout.name +
                                            "' must define all equivalent types or none"};
            }
            if (value.equivalent_type.has_value()) {
                validate_type(*value.equivalent_type, types,
                              "Homogeneous layout '" + layout.name + "' equivalent");
            }
            for (auto const& input : value.input_types) {
                validate_type(input, types,
                              "Homogeneous layout '" + layout.name + "' input");
            }
        }
    }
}

void validate_vector(VectorModuleSchema const& module,
                     std::map<std::string, CppType> const& types) {
    require_value(module.storage_name, "Vector storage name");
    if (module.components.empty() || module.components.size() > 3) {
        throw std::invalid_argument{"Vector module '" + module.settings.name +
                                    "' must have between one and three components"};
    }
    require_unique_names(module.components,
                         "Vector module '" + module.settings.name + "' components");
    validate_type(module.value_type, types, "Vector module '" + module.settings.name + "' value");
    validate_type(module.equivalent_type, types,
                  "Vector module '" + module.settings.name + "' equivalent");
}

void validate_facade(FacadeModuleSchema const& module,
                     std::map<std::string, CppType> const& types) {
    auto const& facade{module.facade};
    require_value(facade.name, "Facade name");
    require_value(facade.target_member_name, "Facade '" + facade.name + "' target member");
    validate_type(facade.target_type, types, "Facade '" + facade.name + "' target");
    if (facade.methods.empty()) {
        throw std::invalid_argument{"Facade '" + facade.name + "' must have methods"};
    }
    for (auto const* access : {&facade.bind_access, &facade.method_access}) {
        if (*access != "public" && *access != "private") {
            throw std::invalid_argument{"Facade '" + facade.name +
                                        "' access must be public or private"};
        }
    }
    for (auto const& method : facade.methods) {
        require_value(method.name, "Facade '" + facade.name + "' method name");
        validate_type(method.return_type, types,
                      "Facade '" + facade.name + "' method '" + method.name + "' return");
        for (auto const& parameter : method.parameters) {
            require_value(parameter.name,
                          "Facade '" + facade.name + "' method parameter");
            validate_type(parameter.type, types,
                          "Facade '" + facade.name + "' method '" + method.name + "' parameter");
        }
    }
}

} // namespace

void validate_manifest(Manifest const& manifest) {
    std::set<std::string> module_names;
    std::set<std::filesystem::path> output_paths;
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
                    if (path.has_value() && !output_paths.insert(path->lexically_normal()).second) {
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
                    require_unique_names(module.headers,
                                         "Umbrella module '" + module.settings.name + "' headers");
                }
            },
            schema);
    }
}

} // namespace codegen::detail
