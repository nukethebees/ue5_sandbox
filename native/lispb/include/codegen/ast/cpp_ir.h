#pragma once

#include <codegen/ast/access.h>
#include <codegen/ast/access_indentation.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/block_id.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/function_formatting.h>
#include <codegen/ast/function_kind.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/ir/access_specifiers.h>
#include <codegen/ast/ir/assignments.h>
#include <codegen/ast/ir/binary_expressions.h>
#include <codegen/ast/ir/block_comments.h>
#include <codegen/ast/ir/blocks.h>
#include <codegen/ast/ir/calls.h>
#include <codegen/ast/ir/casts.h>
#include <codegen/ast/ir/constexpr_members.h>
#include <codegen/ast/ir/enumerators.h>
#include <codegen/ast/ir/enums.h>
#include <codegen/ast/ir/expression_statements.h>
#include <codegen/ast/ir/for_statements.h>
#include <codegen/ast/ir/forward_declarations.h>
#include <codegen/ast/ir/friend_declarations.h>
#include <codegen/ast/ir/function_bodies.h>
#include <codegen/ast/ir/function_nodes.h>
#include <codegen/ast/ir/functions.h>
#include <codegen/ast/ir/if_statements.h>
#include <codegen/ast/ir/includes.h>
#include <codegen/ast/ir/initializer_expressions.h>
#include <codegen/ast/ir/instance_members.h>
#include <codegen/ast/ir/line_breaks.h>
#include <codegen/ast/ir/line_comments.h>
#include <codegen/ast/ir/line_directives.h>
#include <codegen/ast/ir/literals.h>
#include <codegen/ast/ir/member_expressions.h>
#include <codegen/ast/ir/member_initializers.h>
#include <codegen/ast/ir/names.h>
#include <codegen/ast/ir/namespaces.h>
#include <codegen/ast/ir/parameters.h>
#include <codegen/ast/ir/parenthesized_expressions.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/ir/raw_expressions.h>
#include <codegen/ast/ir/raw_lines.h>
#include <codegen/ast/ir/raw_nodes.h>
#include <codegen/ast/ir/records.h>
#include <codegen/ast/ir/returns.h>
#include <codegen/ast/ir/sizeof_expressions.h>
#include <codegen/ast/ir/static_assertions.h>
#include <codegen/ast/ir/static_members.h>
#include <codegen/ast/ir/subscripts.h>
#include <codegen/ast/ir/switch_cases.h>
#include <codegen/ast/ir/switches.h>
#include <codegen/ast/ir/template_id_expressions.h>
#include <codegen/ast/ir/templates.h>
#include <codegen/ast/ir/type_reference_expressions.h>
#include <codegen/ast/ir/type_template_parameters.h>
#include <codegen/ast/ir/unary_expressions.h>
#include <codegen/ast/ir/using_declarations.h>
#include <codegen/ast/ir/value_template_parameters.h>
#include <codegen/ast/ir/variables.h>
#include <codegen/ast/modules.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/record_id.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/render_context.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/template_id.h>
#include <codegen/ast/template_parameter_id.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/lowering/lowering_storage.h>
#include <lispb/indices.h>

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb {
class Strings;
}

namespace lispb::codegen {

class CppTypes;

class CppIR {
  public:
    CppIR(Strings& strings, CppTypes& types)
        : strings{strings}
        , types{types} {}
    CppIR(CppIR const&) = delete;
    auto operator=(CppIR const&) -> CppIR& = delete;
    Strings& strings;
    CppTypes& types;
    LoweringStorage lowering;
    CppFiles files;
    Modules modules;
    auto store_text(std::string_view value) -> StringId;
    auto store_optional_text(std::optional<std::string_view> value) -> std::optional<StringId>;
    void clear();
    auto line_comment(StringId text) -> NodeId;
    auto line_comment(std::string_view text) -> NodeId;
    auto block_comment(StringId text) -> NodeId;
    auto block_comment(std::string_view text) -> NodeId;
    auto line_directive(std::uint32_t line, std::string_view path) -> NodeId;
    auto block(std::span<NodeId const> children) -> BlockId;
    auto block(std::initializer_list<NodeId> children) -> BlockId {
        return block(std::span<NodeId const>{children});
    }
    auto if_statement(Expr condition, BlockId then_block, std::optional<BlockId> else_block = {})
        -> NodeId;
    // Require a variable declaration, assignment or expression statement as the initializer.
    auto for_statement(NodeId initializer, Expr condition, Expr increment, BlockId body) -> NodeId;
    auto switch_case(std::optional<Expr> label, BlockId body) -> SwitchCaseId;
    auto switch_statement(Expr condition, std::span<SwitchCaseId const> cases) -> NodeId;
    auto switch_statement(Expr condition, std::initializer_list<SwitchCaseId> cases) -> NodeId {
        return switch_statement(condition, std::span<SwitchCaseId const>{cases});
    }
    auto break_statement() -> NodeId;
    auto expression_statement(Expr expression, std::span<TypeDependencyId const> dependencies = {})
        -> NodeId;
    auto expression_statement(Expr expression, std::initializer_list<TypeDependencyId> dependencies)
        -> NodeId {
        return expression_statement(expression, std::span<TypeDependencyId const>{dependencies});
    }
    auto return_statement(std::optional<Expr> expression = {},
                          std::span<TypeDependencyId const> dependencies = {}) -> NodeId;
    auto return_statement(std::optional<Expr> expression,
                          std::initializer_list<TypeDependencyId> dependencies) -> NodeId {
        return return_statement(expression, std::span<TypeDependencyId const>{dependencies});
    }
    auto assignment(Expr target, Expr value, std::span<TypeDependencyId const> dependencies = {})
        -> NodeId;
    auto assignment(Expr target, Expr value, std::initializer_list<TypeDependencyId> dependencies)
        -> NodeId {
        return assignment(target, value, std::span<TypeDependencyId const>{dependencies});
    }
    auto variable(CppTypeId type, IdentifierId name, std::optional<Expr> initializer = {})
        -> NodeId;
    auto static_assertion(StringId condition, StringId message) -> NodeId;
    auto access(Access value = Access::public_,
                AccessIndentation indentation = AccessIndentation::outdented) -> NodeId;
    auto include(StringId path, IncludeStyle style = IncludeStyle::inferred) -> NodeId;
    auto include_dependencies() -> NodeId;
    auto forward_declaration(IdentifierId name,
                             RecordKind kind = RecordKind::struct_,
                             TemplateId declaration = {}) -> NodeId;
    auto friend_declaration(IdentifierId name,
                            RecordKind kind = RecordKind::class_,
                            TemplateId declaration = {}) -> NodeId;
    auto friend_type(CppTypeId type) -> NodeId;
    auto using_declaration(IdentifierId name, CppTypeId type, TemplateId declaration = {})
        -> NodeId;
    auto instance_member(CppTypeId type, IdentifierId name, std::optional<Expr> initializer = {})
        -> NodeId;
    auto static_member(CppTypeId type,
                       IdentifierId name,
                       std::optional<Expr> initializer = {},
                       bool is_inline = false,
                       TemplateId template_declaration = {}) -> NodeId;
    // Require a nonempty initializer expression.
    auto constexpr_member(CppTypeId type,
                          IdentifierId name,
                          Expr initializer,
                          bool is_inline = false,
                          TemplateId template_declaration = {}) -> NodeId;
    auto enumerator(IdentifierId name,
                    std::optional<StringId> initializer = {},
                    std::optional<StringId> annotation = {}) -> EnumValueId;
    auto enumeration(IdentifierId name,
                     CppTypeId underlying_type,
                     std::span<EnumValueId const> values,
                     IdentifierId export_specifier = {},
                     bool scoped = true) -> NodeId;
    auto enumeration(IdentifierId name,
                     CppTypeId underlying_type,
                     std::initializer_list<EnumValueId> values,
                     IdentifierId export_specifier = {},
                     bool scoped = true) -> NodeId {
        return enumeration(
            name, underlying_type, std::span<EnumValueId const>{values}, export_specifier, scoped);
    }
    auto record(IdentifierId name,
                std::span<NodeId const> children = {},
                RecordKind kind = RecordKind::struct_) -> RecordId;
    auto record(IdentifierId name,
                std::initializer_list<NodeId> children,
                RecordKind kind = RecordKind::struct_) -> RecordId {
        return record(name, std::span<NodeId const>{children}, kind);
    }
    void set_record_bases(RecordId record, std::span<CppTypeId const> bases);
    void set_record_bases(RecordId record, std::initializer_list<CppTypeId> bases) {
        set_record_bases(record, std::span<CppTypeId const>{bases});
    }
    void set_record_template(RecordId record, TemplateId declaration);
    void set_record_requires_clause(RecordId record, Expr clause);
    void set_record_export_specifier(RecordId record, IdentifierId specifier);
    void set_record_dependencies(RecordId record, std::span<TypeDependencyId const> dependencies);
    void set_record_dependencies(RecordId record,
                                 std::initializer_list<TypeDependencyId> dependencies) {
        set_record_dependencies(record, std::span<TypeDependencyId const>{dependencies});
    }
    auto namespace_node(IdentifierId name, std::span<NodeId const> children) -> NodeId;
    auto namespace_node(IdentifierId name, std::initializer_list<NodeId> children) -> NodeId {
        return namespace_node(name, std::span<NodeId const>{children});
    }
    auto record(Expr name,
                std::span<NodeId const> children = {},
                RecordKind kind = RecordKind::struct_) -> RecordId;
    auto record(Expr name,
                std::initializer_list<NodeId> children,
                RecordKind kind = RecordKind::struct_) -> RecordId {
        return record(name, std::span<NodeId const>{children}, kind);
    }
    auto type_template_parameter(IdentifierId name, CppTypeId default_type = {}, bool pack = false)
        -> TemplateParameterId;
    auto value_template_parameter(CppTypeId type,
                                  IdentifierId name = {},
                                  Expr default_value = {},
                                  bool pack = false) -> TemplateParameterId;
    auto template_declaration(std::span<TemplateParameterId const> parameters) -> TemplateId;
    auto template_declaration(std::initializer_list<TemplateParameterId> parameters) -> TemplateId {
        return template_declaration(std::span<TemplateParameterId const>{parameters});
    }
    auto template_specialization() -> TemplateId;
    // Retain arbitrary parameter-list syntax authored by the source language.
    auto raw_template_declaration(StringId parameters) -> TemplateId;
    // Require an actual return type; use the dedicated constructor/destructor factories otherwise.
    auto function(IdentifierId name, CppTypeId return_type) -> FunctionId;
    auto function(Expr name, CppTypeId return_type) -> FunctionId;
    auto constructor(IdentifierId name, bool is_explicit = false) -> FunctionId;
    auto destructor(IdentifierId name) -> FunctionId;
    auto parameter(CppTypeId type,
                   IdentifierId name = {},
                   std::optional<StringId> default_value = {}) -> FunctionParameterId;
    auto member_initializer(IdentifierId member, StringId value) -> MemberInitializerId;
    void set_function_parameters(FunctionId function,
                                 std::span<FunctionParameterId const> parameters);
    void set_function_parameters(FunctionId function,
                                 std::initializer_list<FunctionParameterId> parameters) {
        set_function_parameters(function, std::span<FunctionParameterId const>{parameters});
    }
    void set_function_body(FunctionId function,
                           std::span<NodeId const> body,
                           std::span<MemberInitializerId const> initializers = {});
    void set_function_body(FunctionId function,
                           std::initializer_list<NodeId> body,
                           std::span<MemberInitializerId const> initializers = {}) {
        set_function_body(function, std::span<NodeId const>{body}, initializers);
    }
    void set_function_body(FunctionId function,
                           std::span<NodeId const> body,
                           std::initializer_list<MemberInitializerId> initializers) {
        set_function_body(function, body, std::span<MemberInitializerId const>{initializers});
    }
    void set_function_body(FunctionId function,
                           std::initializer_list<NodeId> body,
                           std::initializer_list<MemberInitializerId> initializers) {
        set_function_body(function,
                          std::span<NodeId const>{body},
                          std::span<MemberInitializerId const>{initializers});
    }
    void set_function_defaulted(FunctionId function);
    void set_function_deleted(FunctionId function);
    void set_function_dependencies(FunctionId function,
                                   std::span<TypeDependencyId const> dependencies);
    void set_function_dependencies(FunctionId function,
                                   std::initializer_list<TypeDependencyId> dependencies) {
        set_function_dependencies(function, std::span<TypeDependencyId const>{dependencies});
    }
    auto function_flags(FunctionId function) -> FunctionFlags&;
    auto function_formatting(FunctionId function) -> FunctionFormatting&;
    auto function_return_type(FunctionId function) const -> CppTypeId;
    auto function_parameters(FunctionId function) const -> std::span<FunctionParameterId const>;
    auto function_return_syntax(FunctionId function) const -> ReturnSyntax;
    auto function_receiver(FunctionId function) const -> FunctionReceiver;
    auto parameter_name(FunctionParameterId parameter) const -> IdentifierId;
    auto parameter_type(FunctionParameterId parameter) const -> CppTypeId;
    void set_parameter_type(FunctionParameterId parameter, CppTypeId type);
    void set_function_name(FunctionId function, IdentifierId name);
    void set_function_name(FunctionId function, Expr name);
    void set_function_return_type(FunctionId function,
                                  CppTypeId type,
                                  ReturnSyntax syntax = ReturnSyntax::direct);
    void set_function_noexcept(FunctionId function);
    void set_function_may_throw(FunctionId function);
    // Require a nonempty condition expression.
    void set_function_conditional_noexcept(FunctionId function, Expr condition);
    void set_function_receiver(FunctionId function, FunctionReceiver receiver);
    void set_function_export_specifier(FunctionId function, IdentifierId specifier);
    void set_function_template(FunctionId function, TemplateId declaration);
    void set_function_requires_clause(FunctionId function, Expr clause);
    auto raw(StringId value, std::span<TypeDependencyId const> dependencies = {}) -> NodeId;
    auto raw_lines(std::span<StringId const> lines,
                   std::span<TypeDependencyId const> dependencies = {}) -> NodeId;
    auto raw(std::string_view value, std::span<TypeDependencyId const> dependencies = {}) -> NodeId;
    auto raw(std::string_view value, std::initializer_list<TypeDependencyId> dependencies)
        -> NodeId {
        return raw(value, std::span<TypeDependencyId const>{dependencies});
    }
    // Require a positive formatting count.
    auto lines(int count) -> NodeId;
    auto declaration(FunctionId spec) -> NodeId;
    auto header_function(FunctionId spec) -> NodeId;
    auto definition(FunctionId spec,
                    std::optional<StringId> owner = {},
                    std::optional<ReturnSyntax> syntax = {},
                    bool header = false) -> NodeId;
    auto named(std::string_view value, std::span<TypeDependencyId const> dependencies = {}) -> Expr;
    auto named(IdentifierId value, std::span<TypeDependencyId const> dependencies = {}) -> Expr;
    auto named(std::string_view value, std::initializer_list<TypeDependencyId> dependencies)
        -> Expr {
        return named(value, std::span<TypeDependencyId const>{dependencies});
    }
    auto literal(std::string_view value) -> Expr;
    auto literal(StringId value) -> Expr;
    auto string_literal(std::string_view value) -> Expr;
    auto string_literal(StringId value) -> Expr;
    auto raw_expression(StringId value, std::span<TypeDependencyId const> dependencies = {})
        -> Expr;
    auto raw_expression(std::string_view value, std::span<TypeDependencyId const> dependencies = {})
        -> Expr;
    auto raw_expression(std::string_view value,
                        std::initializer_list<TypeDependencyId> dependencies) -> Expr {
        return raw_expression(value, std::span<TypeDependencyId const>{dependencies});
    }
    auto call(Expr callee, std::span<Expr const> arguments = {}) -> Expr;
    auto call(Expr callee, std::initializer_list<Expr> arguments) -> Expr {
        return call(callee, std::span<Expr const>{arguments});
    }
    auto member_access(Expr object, std::string_view member) -> Expr;
    auto member_access(Expr object, IdentifierId member) -> Expr;
    auto pointer_member_access(Expr object, std::string_view member) -> Expr;
    auto pointer_member_access(Expr object, IdentifierId member) -> Expr;
    auto unary(UnaryOperator operation, Expr operand) -> Expr;
    auto subscript(Expr object, Expr index) -> Expr;
    auto binary(BinaryOperator operation, Expr left, Expr right, bool line_break_after = false)
        -> Expr;
    auto type_reference(CppTypeId type) -> Expr;
    auto parenthesized(Expr expression) -> Expr;
    auto template_id(Expr callee, std::span<Expr const> arguments) -> Expr;
    auto template_id(Expr callee, std::initializer_list<Expr> arguments) -> Expr {
        return template_id(callee, std::span<Expr const>{arguments});
    }
    auto static_cast_expr(CppTypeId type, Expr operand) -> Expr;
    auto sizeof_type(CppTypeId type) -> Expr;
    auto init_list(std::span<Expr const> elements, std::optional<CppTypeId> type = {}) -> Expr;
    auto init_list(std::initializer_list<Expr> elements, std::optional<CppTypeId> type = {})
        -> Expr {
        return init_list(std::span<Expr const>{elements}, type);
    }
    auto render(Expr expression) const -> std::string;
    auto render(NodeId node, RenderContext const& context = {}) const -> std::string;
    auto render(CppFileId file) const -> std::string;
    auto render_nodes(std::span<NodeId const> nodes,
                      RenderContext const& context,
                      int newlines) const -> std::string;
    void collect_dependencies(Expr expression, std::vector<TypeDependencyId>& result) const;
    void collect_dependencies(NodeId node, std::vector<TypeDependencyId>& result) const;
    void for_each_child(NodeId node, std::function<void(NodeId)> const& visit) const;
  private:
    auto append_function(Expr name, CppTypeId return_type, FunctionKind kind) -> FunctionId;
    void append_template(std::string& output, TemplateId declaration) const;
    void collect_dependencies(TemplateId declaration, std::vector<TypeDependencyId>& result) const;
    ir::TypeTemplateParameters type_template_parameters_;
    ir::ValueTemplateParameters value_template_parameters_;
    ir::Templates templates_;
    std::vector<TemplateParameterId> template_parameter_references_;
    ir::TemplateIdExpressions template_id_expressions_;
    ir::TypeReferenceExpressions type_reference_expressions_;
    ir::ParenthesizedExpressions parenthesized_expressions_;
    auto function_node(FunctionId function,
                       std::optional<StringId> owner,
                       bool declaration,
                       bool header,
                       std::optional<ReturnSyntax> return_syntax) -> NodeId;
    auto save_nodes(std::span<NodeId const> values) -> ir::Range;
    auto save_expressions(std::span<Expr const> values) -> ir::Range;
    auto save_types(std::span<CppTypeId const> values) -> ir::Range;
    auto save_dependencies(std::span<TypeDependencyId const> values) -> ir::Range;
    auto nodes(ir::Range range) const -> std::span<NodeId const>;
    auto expressions(ir::Range range) const -> std::span<Expr const>;
    auto type_list(ir::Range range) const -> std::span<CppTypeId const>;
    auto dependency_list(ir::Range range) const -> std::span<TypeDependencyId const>;
    auto expression_precedence(Expr expression) const -> int;
    void append_expression(std::string& output,
                           Expr expression,
                           int minimum = 0,
                           bool equal = false) const;
    void append_node(std::string& output, NodeId node, RenderContext const& context) const;
    void append_nodes(std::string& output,
                      std::span<NodeId const> values,
                      RenderContext const& context,
                      int newlines) const;
    auto signature(std::uint32_t function_index, bool record_scope) const -> std::string;
    std::vector<FunctionParameterId> parameter_references_;
    std::vector<MemberInitializerId> initializer_references_;
    std::vector<SwitchCaseId> case_references_;
    std::vector<EnumValueId> enumerator_references_;
    std::vector<NodeId> children_;
    std::vector<Expr> arguments_;
    std::vector<CppTypeId> type_references_;
    std::vector<TypeDependencyId> dependencies_;
    ir::RawNodes raw_nodes_;
    ir::RawLines raw_lines_nodes_;
    std::vector<StringId> raw_lines_;
    ir::LineComments line_comment_nodes_;
    ir::LineDirectives line_directive_nodes_;
    ir::BlockComments block_comment_nodes_;
    ir::Blocks block_nodes_;
    ir::IfStatements if_stmt_nodes_;
    ir::ForStatements for_stmt_nodes_;
    ir::Switches switch_stmt_nodes_;
    ir::ExpressionStatements expression_stmt_nodes_;
    ir::Returns return_stmt_nodes_;
    ir::Assignments assignment_stmt_nodes_;
    ir::Variables variable_declaration_stmt_nodes_;
    ir::StaticAssertions static_assert_nodes_;
    ir::LineBreaks new_lines_nodes_;
    ir::AccessSpecifiers access_specifier_nodes_;
    ir::Includes include_nodes_;
    ir::ForwardDeclarations forward_declaration_nodes_;
    ir::FriendDeclarations friend_declaration_nodes_;
    ir::UsingDeclarations using_declaration_nodes_;
    ir::InstanceMembers instance_members_;
    ir::StaticMembers static_members_;
    ir::ConstexprMembers constexpr_members_;
    ir::FunctionNodes function_nodes_;
    ir::Enums enum_nodes_;
    ir::Records struct_nodes_;
    ir::Namespaces namespace_nodes_;
    ir::Names named_expressions_;
    ir::Literals literal_expressions_;
    std::vector<StringId> string_literal_expressions_;
    ir::Calls call_expressions_;
    ir::MemberExpressions member_expressions_;
    ir::Subscripts subscript_expressions_;
    ir::BinaryExpressions binary_expressions_;
    ir::UnaryExpressions unary_expressions_;
    ir::Casts cast_expressions_;
    ir::SizeofExpressions sizeof_expressions_;
    ir::InitializerExpressions initializer_expressions_;
    ir::RawExpressions raw_expressions_;
    ir::Parameters parameters_;
    ir::MemberInitializers initializers_;
    ir::Enumerators enumerators_;
    ir::SwitchCases cases_;
    ir::Functions functions_;
    ir::FunctionBodies function_bodies_;
    std::vector<Expr> conditional_noexcept_;
};

} // namespace lispb::codegen
