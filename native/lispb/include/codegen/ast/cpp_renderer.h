#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/name_origin.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/render_context.h>
#include <codegen/ast/render_state.h>
#include <codegen/ast/resolved_name.h>
#include <codegen/ast/syntax.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace lispb::codegen {
class CppIR;

// Consume completed C++ IR, build its lookup environments, then emit source text.
class CppRenderer {
  public:
    static constexpr std::string_view default_indent{"    "};

    explicit CppRenderer(CppIR const& ir, std::string_view indent_text = default_indent);
    // Reset lookup state for each entry point; the same renderer can emit multiple files.
    auto render(Expr expr, RenderContext const& ctx = {}) -> std::string;
    auto render(Syntax syntax, RenderContext const& ctx = {}) -> std::string;
    auto render(NodeId node, RenderContext const& ctx = {}) -> std::string;
    auto render(CppFileId file) -> std::string;
    auto render_nodes(std::span<NodeId const> values, RenderContext const& ctx, int newlines)
        -> std::string;
  private:
    /* **************************************** */
    // Formatting
    /* **************************************** */
    void append_indent(std::string& output, RenderContext const& ctx) const;
    void
        append_indented(std::string& output, std::string_view text, RenderContext const& ctx) const;
    auto in_record_scope(EnvironmentId environment) const -> bool;

    /* **************************************** */
    // Types and environments
    /* **************************************** */
    void append_type(std::string& output, CppTypeId type, RenderContext const& ctx);
    auto resolve_name(std::span<StringId const> components,
                      NameOrigin origin,
                      EnvironmentId environment) -> ResolvedName;
    auto alias_scope(SymbolBinding alias) -> EnvironmentId;
    void append_name(std::string& output, std::span<StringId const> components, NameOrigin origin);
    auto declaration_name(Syntax syntax) -> StringId;
    void declare_name(EnvironmentId environment, Syntax syntax, CppDeclarationId declaration);
    void collect_environment(EnvironmentId environment,
                             std::span<NodeId const> children,
                             bool bindings_known = false);
    void collect_template_parameters(EnvironmentId environment, TemplateId declaration);
    auto block_environment(std::span<NodeId const> children,
                           EnvironmentKind kind = EnvironmentKind::block) -> EnvironmentId;
    auto record_environment(std::string_view owner) -> EnvironmentId;
    void initialize_environments(std::span<NodeId const> children);

    /* **************************************** */
    // Expressions and declarations
    /* **************************************** */
    auto expression(Expr expr, RenderContext const& ctx) -> std::string;
    auto expression(Syntax syntax, RenderContext const& ctx) -> std::string;
    auto expr_precedence(Syntax syntax) -> int;
    void append_expr(std::string& output,
                     Syntax syntax,
                     RenderContext const& ctx,
                     int minimum = 0,
                     bool equal = false);
    auto expr_precedence(Expr expr) -> int;
    void append_initializer(std::string& output,
                            Syntax value,
                            CppTypeId destination,
                            RenderContext const& ctx,
                            int minimum = 0);
    void append_initializer_elements(std::string& output,
                                     ir::Range elements,
                                     CppTypeId destination,
                                     RenderContext const& ctx);
    auto append_initializer_literal(std::string& output, Expr value, CppTypeId destination) -> bool;
    void append_expr(std::string& output,
                     Expr expr,
                     RenderContext const& ctx,
                     int minimum = 0,
                     bool equal = false);
    void append_template(std::string& output,
                         TemplateId declaration,
                         RenderContext const& ctx,
                         bool keyword = true);
    void append_parameters(std::string& output,
                           std::span<FunctionParameterId const> parameters,
                           RenderContext const& ctx,
                           bool defaults);
    auto signature(std::uint32_t function_index,
                   RenderContext const& ctx,
                   EnvironmentId& environment) -> std::string;
    void append_node(std::string& output, NodeId node, RenderContext const& ctx);
    void append_nodes(std::string& output,
                      std::span<NodeId const> values,
                      RenderContext const& ctx,
                      int newlines);

    CppIR const& ir_;
    RenderState state_;
    std::string_view indent_text_;
};
}
