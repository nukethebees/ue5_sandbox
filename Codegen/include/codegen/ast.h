#pragma once

#include <filesystem>
#include <concepts>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace codegen {

struct TypeDependency {
    std::string spelling;
    std::optional<std::string> header;
    std::vector<TypeDependency> dependencies;

    auto operator==(TypeDependency const&) const -> bool = default;
};

enum class TypeOperation {
    remove_at_swap,
};

struct CppType {
    std::string spelling;
    std::vector<TypeDependency> dependencies;
    std::map<TypeOperation, std::string> member_operations;

    CppType() = default;
    CppType(char const* spelling);
    CppType(std::string spelling);
    CppType(std::string spelling, std::string header);
    CppType(std::string spelling, std::vector<TypeDependency> dependencies);

    auto operation(TypeOperation operation) const -> std::optional<std::string>;
};

struct Node;
using Nodes = std::vector<Node>;

struct Raw {
    std::string text;
    std::vector<TypeDependency> dependencies;
};

struct NewLines {
    int count;
};

struct Include {
    std::string path;
    std::optional<bool> system;
};

struct IncludeDependencies {};

struct ForwardDeclaration {
    std::string name;
    std::string kind{"struct"};
};

struct UsingDeclaration {
    std::string name;
    CppType type;
};

struct Member {
    CppType type;
    std::string name;
    std::optional<std::string> initializer;

    Member(CppType type, std::string name);
    Member(CppType type, std::string name, std::string initializer);
};

struct FunctionParameter {
    CppType type;
    std::string name;
    std::optional<std::string> default_value;

    FunctionParameter(CppType type, std::string name);
    FunctionParameter(CppType type, std::string name, std::string default_value);
};

struct FunctionSpec {
    std::string name;
    CppType return_type;
    std::vector<FunctionParameter> parameters;
    Nodes body;
    std::string suffix;
    bool is_static{false};
    bool is_inline{false};
    std::optional<std::string> template_parameters;
    std::optional<std::string> requires_clause;
};

struct Function {
    FunctionSpec spec;
    std::optional<std::string> owner;
    bool declaration{false};
    bool is_header{false};
};

struct Struct {
    std::string name;
    Nodes children;
    std::vector<CppType> bases;
    std::optional<std::string> template_parameters;
    std::optional<std::string> export_specifier;
    std::string record_kind{"struct"};
};

struct Namespace {
    std::string name;
    Nodes children;
};

struct Node {
    using Value = std::variant<Raw,
                               NewLines,
                               Include,
                               IncludeDependencies,
                               ForwardDeclaration,
                               UsingDeclaration,
                               Member,
                               Function,
                               Struct,
                               Namespace>;

    Value value;

    template <typename T>
        requires std::constructible_from<Value, T>
    Node(T node) : value{std::move(node)} {}
};

struct RenderContext {
    int indent_level{0};
    std::string indent_text{"    "};
    std::vector<std::vector<Include>> include_groups;

    auto indent() const -> RenderContext;
    auto apply_indent(std::string_view text) const -> std::string;
};

struct CppFile {
    std::filesystem::path path;
    Nodes nodes;
    bool pragma_once{true};
    bool clang_format_off{false};
    std::vector<std::string> prologue;
    std::vector<std::string> epilogue;
    std::vector<std::string> include_order;
};

struct Module {
    std::string name;
    std::optional<CppFile> header;
    std::optional<CppFile> source;
};

auto render(Node const& node, RenderContext const& context = {}) -> std::string;
auto render(CppFile const& file) -> std::string;
auto render_nodes(Nodes const& nodes, RenderContext const& context, int default_newlines)
    -> std::string;
auto children(Node const& node) -> Nodes const*;
auto dependencies(Node const& node) -> std::vector<TypeDependency>;
auto include_is_system(Include const& include) -> bool;

auto raw(std::string text) -> Node;
auto raw(std::string text, std::vector<TypeDependency> dependencies) -> Node;
auto lines(int count) -> Node;
auto declaration(FunctionSpec spec) -> Node;
auto header_function(FunctionSpec spec) -> Node;
auto definition(FunctionSpec spec, std::string owner) -> Node;

} // namespace codegen
