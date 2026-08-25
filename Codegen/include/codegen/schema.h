#pragma once

#include <codegen/ast.h>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace codegen {

enum class StorageOperation {
    reset,
    reserve,
    add_uninitialised,
    add_defaulted,
    remove_at_swap,
    set_num,
    copy_element,
    append_from,
};

struct TypeRef {
    std::string name;
    std::string suffix;
    std::optional<std::string> nested;
};

struct ParameterSchema {
    TypeRef type;
    std::string name;
    std::optional<std::string> default_value;
};

struct FunctionSchema {
    std::string name;
    TypeRef return_type;
    std::vector<ParameterSchema> parameters;
    std::vector<std::string> body_lines;
    std::vector<std::string> dependencies;
    std::string suffix;
    bool is_static{false};
    bool is_inline{false};
    bool definition_in_source{false};
    std::optional<std::string> template_parameters;
    std::optional<std::string> requires_clause;
};

enum class SoaMemberKind {
    array,
    nested,
};

struct SoaMemberSchema {
    std::string name;
    SoaMemberKind kind;
    TypeRef type;
    std::optional<std::string> fixed_schema;
};

struct FixedSoaSchema {
    std::string storage_name;
    std::vector<std::string> containers;
};

struct SoaSchema {
    std::string name;
    std::optional<std::string> view_name;
    std::optional<std::string> const_view_name;
    std::vector<SoaMemberSchema> members;
    std::vector<StorageOperation> operations;
    std::optional<std::string> export_specifier;
    std::vector<FunctionSchema> functions;
    std::vector<std::string> using_declarations;
    std::optional<TypeRef> equivalent_type;
    bool copy_element_memberwise{false};
    std::optional<FixedSoaSchema> fixed;
};

struct ModuleSettings {
    std::string name;
    std::filesystem::path header;
    std::optional<std::filesystem::path> source;
    std::optional<std::string> header_include;
    std::optional<std::string> namespace_name;
    std::vector<std::string> include_order;
    std::vector<std::string> prelude_lines;
};

struct SoaModuleSchema {
    ModuleSettings settings;
    std::vector<SoaSchema> structs;
};

struct HomogeneousValueSchema {
    TypeRef type;
    std::string suffix;
    std::optional<TypeRef> equivalent_type;
    std::vector<TypeRef> input_types;
};

struct HomogeneousLayoutSchema {
    std::string name;
    std::vector<std::string> components;
    std::vector<HomogeneousValueSchema> value_types;
    std::optional<std::string> export_specifier;
};

struct HomogeneousModuleSchema {
    ModuleSettings settings;
    std::vector<HomogeneousLayoutSchema> layouts;
};

struct VectorModuleSchema {
    ModuleSettings settings;
    std::string storage_name;
    TypeRef value_type;
    std::vector<std::string> components;
    TypeRef equivalent_type;
    std::optional<std::string> export_specifier;
    std::optional<FixedSoaSchema> fixed;
};

struct FacadeMethodSchema {
    std::string name;
    TypeRef return_type;
    std::vector<ParameterSchema> parameters;
    std::string suffix;
    std::optional<std::string> target_name;
};

struct FacadeSchema {
    std::string name;
    TypeRef target_type;
    std::string target_member_name;
    std::vector<FacadeMethodSchema> methods;
    std::vector<std::string> validation_lines;
    std::vector<std::string> validation_dependencies;
    std::optional<std::string> export_specifier;
    std::string bind_access{"public"};
    std::string method_access{"public"};
    std::vector<std::string> friends;
    bool definitions_in_source{false};
};

struct FacadeModuleSchema {
    ModuleSettings settings;
    FacadeSchema facade;
};

struct UmbrellaModuleSchema {
    ModuleSettings settings;
    std::vector<std::string> headers;
};

using ModuleSchema = std::variant<SoaModuleSchema,
                                  HomogeneousModuleSchema,
                                  VectorModuleSchema,
                                  FacadeModuleSchema,
                                  UmbrellaModuleSchema>;

struct Manifest {
    int schema_version;
    std::map<std::string, CppType> types;
    std::vector<ModuleSchema> modules;
};

auto resolve_type(TypeRef const& reference, std::map<std::string, CppType> const& types)
    -> CppType;
auto all_storage_operations() -> std::vector<StorageOperation>;

} // namespace codegen
