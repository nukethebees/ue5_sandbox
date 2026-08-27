#pragma once

#include <codegen/ast/access_specifier.h>
#include <codegen/ast/assignment_statement.h>
#include <codegen/ast/cpp_file.h>
#include <codegen/ast/cpp_type.h>
#include <codegen/ast/expression_statement.h>
#include <codegen/ast/forward_declaration.h>
#include <codegen/ast/friend_declaration.h>
#include <codegen/ast/function.h>
#include <codegen/ast/function_formatting.h>
#include <codegen/ast/function_parameter.h>
#include <codegen/ast/function_spec.h>
#include <codegen/ast/include.h>
#include <codegen/ast/include_dependencies.h>
#include <codegen/ast/member.h>
#include <codegen/ast/module.h>
#include <codegen/ast/namespace.h>
#include <codegen/ast/new_lines.h>
#include <codegen/ast/node.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/raw.h>
#include <codegen/ast/render_context.h>
#include <codegen/ast/return_statement.h>
#include <codegen/ast/struct.h>
#include <codegen/ast/type_dependency.h>
#include <codegen/ast/type_operation.h>
#include <codegen/ast/using_declaration.h>

#include <string>

namespace codegen {

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
