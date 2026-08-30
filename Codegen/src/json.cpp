#include <codegen/json.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>
#include <sstream>
#include <string_view>

namespace codegen {
namespace {

using Json = nlohmann::json;

auto read_json(std::filesystem::path const& path) -> Json {
    std::ifstream input{path};
    if (!input) {
        throw ManifestError{"Cannot open manifest file: " + path.string()};
    }
    try {
        std::vector<std::set<std::string>> object_keys;
        auto duplicate_key = [&](int, Json::parse_event_t const event, Json& parsed) {
            if (event == Json::parse_event_t::object_start) {
                object_keys.emplace_back();
            } else if (event == Json::parse_event_t::key) {
                auto const key{parsed.get<std::string>()};
                if (!object_keys.back().insert(key).second) {
                    throw ManifestError{path.string() + ": duplicate field '" + key + "'"};
                }
            } else if (event == Json::parse_event_t::object_end) {
                object_keys.pop_back();
            }
            return true;
        };
        return Json::parse(input, duplicate_key);
    } catch (Json::exception const& error) {
        throw ManifestError{path.string() + ": " + error.what()};
    }
}

void require_object(Json const& value, std::string const& path) {
    if (!value.is_object()) {
        throw ManifestError{path + " must be an object; got " + value.dump()};
    }
}

void require_array(Json const& value, std::string const& path) {
    if (!value.is_array()) {
        throw ManifestError{path + " must be an array; got " + value.dump()};
    }
}

auto required_value(Json const& value, char const* key, std::string const& path) -> Json const& {
    auto const iterator{value.find(key)};
    if (iterator == value.end()) {
        throw ManifestError{path + ": missing required field '" + key + "'"};
    }
    return *iterator;
}

auto required_array(Json const& value, char const* key, std::string const& path) -> Json const& {
    auto const& result{required_value(value, key, path)};
    require_array(result, path + "/" + key);
    return result;
}

auto required_object(Json const& value, char const* key, std::string const& path) -> Json const& {
    auto const& result{required_value(value, key, path)};
    require_object(result, path + "/" + key);
    return result;
}

auto optional_array(Json const& value, char const* key, std::string const& path) -> Json const* {
    auto const iterator{value.find(key)};
    if (iterator == value.end()) {
        return nullptr;
    }
    require_array(*iterator, path + "/" + key);
    return &*iterator;
}

void reject_unknown(Json const& value,
                    std::string const& path,
                    std::initializer_list<std::string_view> allowed) {
    require_object(value, path);
    std::set<std::string_view> const allowed_fields{allowed};
    for (auto const& [key, unused] : value.items()) {
        static_cast<void>(unused);
        if (!allowed_fields.contains(key)) {
            throw ManifestError{path + ": unknown field '" + key + "'"};
        }
    }
}

template <typename T>
auto required(Json const& value, char const* key, std::string const& path) -> T {
    try {
        return required_value(value, key, path).get<T>();
    } catch (Json::exception const& error) {
        throw ManifestError{path + "/" + key + ": " + error.what()};
    }
}

template <typename T>
auto optional(Json const& value, char const* key, std::string const& path) -> std::optional<T> {
    auto const iterator{value.find(key)};
    if (iterator == value.end()) {
        return std::nullopt;
    }
    try {
        return iterator->get<T>();
    } catch (Json::exception const& error) {
        throw ManifestError{path + "/" + key + ": " + error.what()};
    }
}

template <typename T>
auto value_or(Json const& value, char const* key, T fallback, std::string const& path) -> T {
    auto parsed{optional<T>(value, key, path)};
    return parsed.has_value() ? std::move(*parsed) : std::move(fallback);
}

auto parse_type_ref(Json const& value, std::string const& path) -> TypeRef {
    if (value.is_string()) {
        return TypeRef{value.get<std::string>(), {}, std::nullopt};
    }
    reject_unknown(value, path, {"name", "suffix", "nested"});
    return TypeRef{
        .name = required<std::string>(value, "name", path),
        .suffix = value_or<std::string>(value, "suffix", {}, path),
        .nested = optional<std::string>(value, "nested", path),
    };
}

auto parse_parameter(Json const& value, std::string const& path) -> ParameterSchema {
    reject_unknown(value, path, {"type", "name", "default"});
    return ParameterSchema{
        .type = parse_type_ref(required_value(value, "type", path), path + "/type"),
        .name = required<std::string>(value, "name", path),
        .default_value = optional<std::string>(value, "default", path),
    };
}

auto parse_function(Json const& value, std::string const& path) -> FunctionSchema {
    reject_unknown(value,
                   path,
                   {"name",
                    "return_type",
                    "parameters",
                    "body",
                    "dependencies",
                    "trailing_return_type",
                    "const",
                    "noexcept",
                    "static",
                    "inline",
                    "definition_in_source",
                    "template_parameters",
                    "requires"});
    std::vector<ParameterSchema> parameters;
    auto const* parameter_values{optional_array(value, "parameters", path)};
    auto const parameter_count{parameter_values == nullptr ? 0 : parameter_values->size()};
    for (std::size_t index{0}; index < parameter_count; ++index) {
        parameters.push_back(parse_parameter((*parameter_values)[index],
                                             path + "/parameters/" + std::to_string(index)));
    }
    return FunctionSchema{
        .name = required<std::string>(value, "name", path),
        .return_type =
            parse_type_ref(required_value(value, "return_type", path), path + "/return_type"),
        .parameters = std::move(parameters),
        .body_lines = value_or<std::vector<std::string>>(value, "body", {}, path),
        .dependencies = value_or<std::vector<std::string>>(value, "dependencies", {}, path),
        .trailing_return_type =
            value.contains("trailing_return_type")
                ? std::optional<TypeRef>{parse_type_ref(value.at("trailing_return_type"),
                                                        path + "/trailing_return_type")}
                : std::nullopt,
        .is_const = value_or<bool>(value, "const", false, path),
        .is_noexcept = value_or<bool>(value, "noexcept", false, path),
        .is_static = value_or<bool>(value, "static", false, path),
        .is_inline = value_or<bool>(value, "inline", false, path),
        .definition_in_source = value_or<bool>(value, "definition_in_source", false, path),
        .template_parameters = optional<std::string>(value, "template_parameters", path),
        .requires_clause = optional<std::string>(value, "requires", path),
    };
}

auto parse_operation(std::string const& value, std::string const& path) -> StorageOperation {
    static std::map<std::string, StorageOperation> const operations{
        {"reset", StorageOperation::reset},
        {"reserve", StorageOperation::reserve},
        {"add_uninitialised", StorageOperation::add_uninitialised},
        {"add_defaulted", StorageOperation::add_defaulted},
        {"remove_at_swap", StorageOperation::remove_at_swap},
        {"set_num", StorageOperation::set_num},
        {"copy_element", StorageOperation::copy_element},
        {"append_from", StorageOperation::append_from},
    };
    auto const found{operations.find(value)};
    if (found == operations.end()) {
        throw ManifestError{path + ": unknown storage operation '" + value + "'"};
    }
    return found->second;
}

auto parse_fixed(Json const& value, std::string const& path) -> FixedSoaSchema {
    reject_unknown(value, path, {"storage_name", "containers"});
    return FixedSoaSchema{
        .storage_name = required<std::string>(value, "storage_name", path),
        .containers = value_or<std::vector<std::string>>(value, "containers", {}, path),
    };
}

auto parse_soa_member(Json const& value, std::string const& path) -> SoaMemberSchema {
    reject_unknown(value, path, {"name", "kind", "type", "fixed_schema"});
    auto const kind_name{required<std::string>(value, "kind", path)};
    SoaMemberKind kind;
    if (kind_name == "array") {
        kind = SoaMemberKind::array;
    } else if (kind_name == "nested") {
        kind = SoaMemberKind::nested;
    } else {
        throw ManifestError{path + ": unknown SOA member kind '" + kind_name + "'"};
    }
    return SoaMemberSchema{
        .name = required<std::string>(value, "name", path),
        .kind = kind,
        .type = parse_type_ref(required_value(value, "type", path), path + "/type"),
        .fixed_schema = optional<std::string>(value, "fixed_schema", path),
    };
}

auto parse_soa(Json const& value, std::string const& path) -> SoaSchema {
    reject_unknown(value,
                   path,
                   {"name",
                    "view_name",
                    "const_view_name",
                    "members",
                    "operations",
                    "export_specifier",
                    "functions",
                    "using_declarations",
                    "equivalent_type",
                    "copy_element_memberwise",
                    "fixed"});
    std::vector<SoaMemberSchema> members;
    auto const& member_values{required_array(value, "members", path)};
    for (std::size_t index{0}; index < member_values.size(); ++index) {
        members.push_back(
            parse_soa_member(member_values[index], path + "/members/" + std::to_string(index)));
    }

    std::vector<StorageOperation> operations;
    auto const operation_names{value_or<std::vector<std::string>>(value, "operations", {}, path)};
    for (std::size_t index{0}; index < operation_names.size(); ++index) {
        if (operation_names[index] == "all") {
            if (operation_names.size() != 1) {
                throw ManifestError{path + "/operations: 'all' must be the only value"};
            }
            operations = all_storage_operations();
            break;
        }
        operations.push_back(
            parse_operation(operation_names[index], path + "/operations/" + std::to_string(index)));
    }

    std::vector<FunctionSchema> functions;
    auto const* function_values{optional_array(value, "functions", path)};
    auto const function_count{function_values == nullptr ? 0 : function_values->size()};
    for (std::size_t index{0}; index < function_count; ++index) {
        functions.push_back(parse_function((*function_values)[index],
                                           path + "/functions/" + std::to_string(index)));
    }

    std::optional<TypeRef> equivalent_type;
    if (value.contains("equivalent_type")) {
        equivalent_type = parse_type_ref(value.at("equivalent_type"), path + "/equivalent_type");
    }
    std::optional<FixedSoaSchema> fixed;
    if (value.contains("fixed")) {
        fixed = parse_fixed(value.at("fixed"), path + "/fixed");
    }
    return SoaSchema{
        .name = required<std::string>(value, "name", path),
        .view_name = optional<std::string>(value, "view_name", path),
        .const_view_name = optional<std::string>(value, "const_view_name", path),
        .members = std::move(members),
        .operations = std::move(operations),
        .export_specifier = optional<std::string>(value, "export_specifier", path),
        .functions = std::move(functions),
        .using_declarations =
            value_or<std::vector<std::string>>(value, "using_declarations", {}, path),
        .equivalent_type = std::move(equivalent_type),
        .copy_element_memberwise = value_or<bool>(value, "copy_element_memberwise", false, path),
        .fixed = std::move(fixed),
    };
}

auto parse_settings(Json const& value, std::string const& path) -> ModuleSettings {
    return ModuleSettings{
        .name = required<std::string>(value, "name", path),
        .header = required<std::string>(value, "header", path),
        .source = optional<std::string>(value, "source", path),
        .header_include = optional<std::string>(value, "header_include", path),
        .namespace_name = optional<std::string>(value, "namespace", path),
        .include_order = value_or<std::vector<std::string>>(value, "include_order", {}, path),
        .prelude_lines = value_or<std::vector<std::string>>(value, "prelude", {}, path),
    };
}

auto parse_facade_method(Json const& value, std::string const& path) -> FacadeMethodSchema {
    reject_unknown(
        value, path, {"name", "return_type", "parameters", "const", "noexcept", "target_name"});
    std::vector<ParameterSchema> parameters;
    auto const* parameter_values{optional_array(value, "parameters", path)};
    auto const parameter_count{parameter_values == nullptr ? 0 : parameter_values->size()};
    for (std::size_t index{0}; index < parameter_count; ++index) {
        parameters.push_back(parse_parameter((*parameter_values)[index],
                                             path + "/parameters/" + std::to_string(index)));
    }
    return FacadeMethodSchema{
        .name = required<std::string>(value, "name", path),
        .return_type =
            parse_type_ref(required_value(value, "return_type", path), path + "/return_type"),
        .parameters = std::move(parameters),
        .is_const = value_or<bool>(value, "const", false, path),
        .is_noexcept = value_or<bool>(value, "noexcept", false, path),
        .target_name = optional<std::string>(value, "target_name", path),
    };
}

auto parse_enum_reflection(std::string const& value, std::string const& path) -> EnumReflection {
    if (value == "none") {
        return EnumReflection::none;
    }
    if (value == "uenum") {
        return EnumReflection::uenum;
    }
    if (value == "blueprint") {
        return EnumReflection::blueprint;
    }
    throw ManifestError{path + ": unknown enum reflection mode '" + value + "'"};
}

auto parse_enum_conversion(std::string const& value, std::string const& path) -> EnumConversion {
    if (value == "lex_to_string") {
        return EnumConversion::lex_to_string;
    }
    if (value == "string_view") {
        return EnumConversion::string_view;
    }
    if (value == "string") {
        return EnumConversion::string;
    }
    if (value == "lex_to_display_string") {
        return EnumConversion::lex_to_display_string;
    }
    if (value == "display_string_view") {
        return EnumConversion::display_string_view;
    }
    if (value == "display_string") {
        return EnumConversion::display_string;
    }
    throw ManifestError{path + ": unknown enum conversion '" + value + "'"};
}

auto parse_enum(Json const& value, std::string const& path) -> EnumSchema {
    reject_unknown(value,
                   path,
                   {"name",
                    "underlying_type",
                    "reflection",
                    "values",
                    "conversions",
                    "export_specifier"});
    std::vector<EnumeratorSchema> values;
    auto const& value_entries{required_array(value, "values", path)};
    for (std::size_t index{0}; index < value_entries.size(); ++index) {
        auto const& entry{value_entries[index]};
        auto const entry_path{path + "/values/" + std::to_string(index)};
        reject_unknown(entry, entry_path, {"name", "value", "display_name", "hidden"});
        values.push_back(EnumeratorSchema{
            .name = required<std::string>(entry, "name", entry_path),
            .initializer = optional<std::string>(entry, "value", entry_path),
            .display_name = optional<std::string>(entry, "display_name", entry_path),
            .hidden = value_or<bool>(entry, "hidden", false, entry_path),
        });
    }

    std::vector<EnumConversion> conversions;
    auto const conversion_names{
        value_or<std::vector<std::string>>(value, "conversions", {}, path)};
    for (std::size_t index{0}; index < conversion_names.size(); ++index) {
        conversions.push_back(parse_enum_conversion(
            conversion_names[index], path + "/conversions/" + std::to_string(index)));
    }
    auto const reflection_name{value_or<std::string>(value, "reflection", "none", path)};
    return EnumSchema{
        .name = required<std::string>(value, "name", path),
        .underlying_type = parse_type_ref(required_value(value, "underlying_type", path),
                                          path + "/underlying_type"),
        .reflection = parse_enum_reflection(reflection_name, path + "/reflection"),
        .values = std::move(values),
        .conversions = std::move(conversions),
        .export_specifier = optional<std::string>(value, "export_specifier", path),
    };
}

auto parse_module(Json const& value, std::string const& path) -> ModuleSchema {
    auto const kind{required<std::string>(value, "kind", path)};
    auto settings{parse_settings(value, path)};
    if (kind == "enum") {
        reject_unknown(value,
                       path,
                       {"kind",
                        "name",
                        "header",
                        "source",
                        "header_include",
                        "namespace",
                        "helper_namespace",
                        "include_order",
                        "prelude",
                        "enums"});
        std::vector<EnumSchema> enums;
        auto const& values{required_array(value, "enums", path)};
        for (std::size_t index{0}; index < values.size(); ++index) {
            enums.push_back(
                parse_enum(values[index], path + "/enums/" + std::to_string(index)));
        }
        return EnumModuleSchema{
            .settings = std::move(settings),
            .helper_namespace = optional<std::string>(value, "helper_namespace", path),
            .enums = std::move(enums),
        };
    }
    if (kind == "soa") {
        reject_unknown(value,
                       path,
                       {"kind",
                        "name",
                        "header",
                        "source",
                        "header_include",
                        "namespace",
                        "include_order",
                        "prelude",
                        "structs"});
        std::vector<SoaSchema> structs;
        auto const& values{required_array(value, "structs", path)};
        for (std::size_t index{0}; index < values.size(); ++index) {
            structs.push_back(parse_soa(values[index], path + "/structs/" + std::to_string(index)));
        }
        return SoaModuleSchema{std::move(settings), std::move(structs)};
    }
    if (kind == "facade") {
        reject_unknown(value,
                       path,
                       {"kind",
                        "name",
                        "header",
                        "source",
                        "header_include",
                        "namespace",
                        "include_order",
                        "prelude",
                        "facade"});
        auto const& facade_value{required_object(value, "facade", path)};
        auto const facade_path{path + "/facade"};
        reject_unknown(facade_value,
                       facade_path,
                       {"name",
                        "target_type",
                        "target_member_name",
                        "methods",
                        "validation",
                        "validation_dependencies",
                        "export_specifier",
                        "bind_access",
                        "method_access",
                        "friends",
                        "definitions_in_source"});
        std::vector<FacadeMethodSchema> methods;
        auto const& method_values{required_array(facade_value, "methods", facade_path)};
        for (std::size_t index{0}; index < method_values.size(); ++index) {
            methods.push_back(parse_facade_method(
                method_values[index], facade_path + "/methods/" + std::to_string(index)));
        }
        return FacadeModuleSchema{
            std::move(settings),
            FacadeSchema{
                .name = required<std::string>(facade_value, "name", facade_path),
                .target_type =
                    parse_type_ref(required_value(facade_value, "target_type", facade_path),
                                   facade_path + "/target_type"),
                .target_member_name =
                    required<std::string>(facade_value, "target_member_name", facade_path),
                .methods = std::move(methods),
                .validation_lines =
                    value_or<std::vector<std::string>>(facade_value, "validation", {}, facade_path),
                .validation_dependencies = value_or<std::vector<std::string>>(
                    facade_value, "validation_dependencies", {}, facade_path),
                .export_specifier =
                    optional<std::string>(facade_value, "export_specifier", facade_path),
                .bind_access =
                    value_or<std::string>(facade_value, "bind_access", "public", facade_path),
                .method_access =
                    value_or<std::string>(facade_value, "method_access", "public", facade_path),
                .friends =
                    value_or<std::vector<std::string>>(facade_value, "friends", {}, facade_path),
                .definitions_in_source =
                    value_or<bool>(facade_value, "definitions_in_source", false, facade_path),
            },
        };
    }
    if (kind == "umbrella") {
        reject_unknown(value,
                       path,
                       {"kind",
                        "name",
                        "header",
                        "source",
                        "header_include",
                        "namespace",
                        "include_order",
                        "prelude",
                        "headers"});
        return UmbrellaModuleSchema{
            std::move(settings),
            required<std::vector<std::string>>(value, "headers", path),
        };
    }
    if (kind == "vector_soa") {
        reject_unknown(value,
                       path,
                       {"kind",
                        "name",
                        "header",
                        "source",
                        "header_include",
                        "namespace",
                        "include_order",
                        "prelude",
                        "storage_name",
                        "value_type",
                        "components",
                        "equivalent_type",
                        "export_specifier",
                        "fixed"});
        std::optional<FixedSoaSchema> fixed;
        if (value.contains("fixed")) {
            fixed = parse_fixed(value.at("fixed"), path + "/fixed");
        }
        return VectorModuleSchema{
            .settings = std::move(settings),
            .storage_name = required<std::string>(value, "storage_name", path),
            .value_type =
                parse_type_ref(required_value(value, "value_type", path), path + "/value_type"),
            .components = required<std::vector<std::string>>(value, "components", path),
            .equivalent_type = parse_type_ref(required_value(value, "equivalent_type", path),
                                              path + "/equivalent_type"),
            .export_specifier = optional<std::string>(value, "export_specifier", path),
            .fixed = std::move(fixed),
        };
    }
    if (kind == "homogeneous_soa") {
        reject_unknown(value,
                       path,
                       {"kind",
                        "name",
                        "header",
                        "source",
                        "header_include",
                        "namespace",
                        "include_order",
                        "prelude",
                        "layouts"});
        std::vector<HomogeneousLayoutSchema> layouts;
        auto const& layout_values{required_array(value, "layouts", path)};
        for (std::size_t layout_index{0}; layout_index < layout_values.size(); ++layout_index) {
            auto const& layout{layout_values[layout_index]};
            auto const layout_path{path + "/layouts/" + std::to_string(layout_index)};
            reject_unknown(
                layout, layout_path, {"name", "components", "value_types", "export_specifier"});
            std::vector<HomogeneousValueSchema> value_types;
            auto const& type_values{required_array(layout, "value_types", layout_path)};
            for (std::size_t type_index{0}; type_index < type_values.size(); ++type_index) {
                auto const& type{type_values[type_index]};
                auto const type_path{layout_path + "/value_types/" + std::to_string(type_index)};
                reject_unknown(
                    type, type_path, {"type", "suffix", "equivalent_type", "input_types"});
                std::optional<TypeRef> equivalent;
                if (type.contains("equivalent_type")) {
                    equivalent =
                        parse_type_ref(type.at("equivalent_type"), type_path + "/equivalent_type");
                }
                std::vector<TypeRef> inputs;
                auto const* input_values{optional_array(type, "input_types", type_path)};
                auto const input_count{input_values == nullptr ? 0 : input_values->size()};
                for (std::size_t input_index{0}; input_index < input_count; ++input_index) {
                    inputs.push_back(
                        parse_type_ref((*input_values)[input_index],
                                       type_path + "/input_types/" + std::to_string(input_index)));
                }
                value_types.push_back(HomogeneousValueSchema{
                    .type = parse_type_ref(required_value(type, "type", type_path),
                                           type_path + "/type"),
                    .suffix = required<std::string>(type, "suffix", type_path),
                    .equivalent_type = std::move(equivalent),
                    .input_types = std::move(inputs),
                });
            }
            layouts.push_back(HomogeneousLayoutSchema{
                .name = required<std::string>(layout, "name", layout_path),
                .components = required<std::vector<std::string>>(layout, "components", layout_path),
                .value_types = std::move(value_types),
                .export_specifier = optional<std::string>(layout, "export_specifier", layout_path),
            });
        }
        return HomogeneousModuleSchema{std::move(settings), std::move(layouts)};
    }
    throw ManifestError{path + ": unknown module kind '" + kind + "'"};
}

auto load_types(std::filesystem::path const& path) -> std::map<std::string, CppType> {
    auto const document = read_json(path);
    reject_unknown(document, path.string(), {"types"});
    std::map<std::string, CppType> result;
    for (auto const& [key, value] : required_object(document, "types", path.string()).items()) {
        auto const item_path{path.string() + "/types/" + key};
        reject_unknown(value, item_path, {"spelling", "header", "operations"});
        auto type{CppType{required<std::string>(value, "spelling", item_path)}};
        if (auto header{optional<std::string>(value, "header", item_path)}; header.has_value()) {
            type.dependencies.push_back(TypeDependency{type.spelling, std::move(header), {}});
        }
        if (value.contains("operations")) {
            auto const& operations{value.at("operations")};
            reject_unknown(operations, item_path + "/operations", {"remove_at_swap"});
            if (auto operation{
                    optional<std::string>(operations, "remove_at_swap", item_path + "/operations")};
                operation.has_value()) {
                type.member_operations.emplace(TypeOperation::remove_at_swap,
                                               std::move(*operation));
            }
        }
        result.emplace(key, std::move(type));
    }
    return result;
}

} // namespace

auto load_manifest(std::filesystem::path const& path) -> Manifest {
    auto const document = read_json(path);
    auto const root_path{path.string()};
    reject_unknown(document, root_path, {"schema_version", "types", "modules"});
    auto const version{required<int>(document, "schema_version", root_path)};
    if (version != manifest_schema_version) {
        if (version == 1) {
            throw ManifestError{root_path +
                                "/schema_version: schema version 1 is obsolete; version " +
                                std::to_string(manifest_schema_version) +
                                " replaces function "
                                "'suffix' with 'const' and 'noexcept' fields"};
        }
        throw ManifestError{root_path + "/schema_version: unsupported schema version " +
                            std::to_string(version) + "; expected " +
                            std::to_string(manifest_schema_version)};
    }
    auto const directory{path.parent_path()};
    auto types{load_types(directory / required<std::string>(document, "types", root_path))};
    std::vector<ModuleSchema> modules;
    auto const module_files{required<std::vector<std::string>>(document, "modules", root_path)};
    for (auto const& module_file : module_files) {
        auto const module_path{directory / module_file};
        auto const module_document = read_json(module_path);
        reject_unknown(module_document, module_path.string(), {"modules"});
        auto const& values{required_array(module_document, "modules", module_path.string())};
        for (std::size_t index{0}; index < values.size(); ++index) {
            modules.push_back(parse_module(
                values[index], module_path.string() + "/modules/" + std::to_string(index)));
        }
    }
    return Manifest{version, std::move(types), std::move(modules)};
}

} // namespace codegen
