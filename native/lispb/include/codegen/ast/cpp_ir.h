#pragma once

#include <ioj/enum_array.h>

#include <codegen/ast/access.h>
#include <codegen/ast/access_indentation.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/block_id.h>
#include <codegen/ast/cpp_files.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_definition.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/function_formatting.h>
#include <codegen/ast/function_kind.h>
#include <codegen/ast/function_parameter_view.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/function_view.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/ir/access_specifiers.h>
#include <codegen/ast/ir/assignments.h>
#include <codegen/ast/ir/binary_exprs.h>
#include <codegen/ast/ir/block_comments.h>
#include <codegen/ast/ir/blocks.h>
#include <codegen/ast/ir/calls.h>
#include <codegen/ast/ir/casts.h>
#include <codegen/ast/ir/conditional_exprs.h>
#include <codegen/ast/ir/constexpr_members.h>
#include <codegen/ast/ir/declaration_macros.h>
#include <codegen/ast/ir/designated_initializers.h>
#include <codegen/ast/ir/enumerators.h>
#include <codegen/ast/ir/enums.h>
#include <codegen/ast/ir/expr_stmts.h>
#include <codegen/ast/ir/for_stmts.h>
#include <codegen/ast/ir/forward_declarations.h>
#include <codegen/ast/ir/friend_declarations.h>
#include <codegen/ast/ir/function_bodies.h>
#include <codegen/ast/ir/function_nodes.h>
#include <codegen/ast/ir/functions.h>
#include <codegen/ast/ir/if_stmts.h>
#include <codegen/ast/ir/includes.h>
#include <codegen/ast/ir/initializer_exprs.h>
#include <codegen/ast/ir/instance_members.h>
#include <codegen/ast/ir/lambda_exprs.h>
#include <codegen/ast/ir/line_breaks.h>
#include <codegen/ast/ir/line_comments.h>
#include <codegen/ast/ir/line_directives.h>
#include <codegen/ast/ir/literals.h>
#include <codegen/ast/ir/member_exprs.h>
#include <codegen/ast/ir/member_initializers.h>
#include <codegen/ast/ir/names.h>
#include <codegen/ast/ir/namespaces.h>
#include <codegen/ast/ir/parameters.h>
#include <codegen/ast/ir/parenthesized_exprs.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/ir/raw_exprs.h>
#include <codegen/ast/ir/raw_lines.h>
#include <codegen/ast/ir/raw_nodes.h>
#include <codegen/ast/ir/records.h>
#include <codegen/ast/ir/returns.h>
#include <codegen/ast/ir/sizeof_exprs.h>
#include <codegen/ast/ir/static_assertions.h>
#include <codegen/ast/ir/static_members.h>
#include <codegen/ast/ir/subscripts.h>
#include <codegen/ast/ir/switch_cases.h>
#include <codegen/ast/ir/switches.h>
#include <codegen/ast/ir/template_id_exprs.h>
#include <codegen/ast/ir/templates.h>
#include <codegen/ast/ir/type_template_parameters.h>
#include <codegen/ast/ir/unary_exprs.h>
#include <codegen/ast/ir/using_declarations.h>
#include <codegen/ast/ir/value_template_parameters.h>
#include <codegen/ast/ir/variables.h>
#include <codegen/ast/modules.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/record_definition.h>
#include <codegen/ast/record_id.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/record_view.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/template_id.h>
#include <codegen/ast/template_parameter_id.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/lowering/lowering_storage.h>
#include <lispb/indices.h>

#include <array>
#include <concepts>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lispb {
class Strings;
}

namespace lispb::codegen {

class CppTypes;
class CppRenderer;

struct VariableOptions {
    TemplateId template_declaration{};
    bool is_inline{};
    bool is_constexpr{};
};

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
    auto node_list() -> NodeListBuilder { return NodeListBuilder{*this}; }
    auto lowering_storage() -> LoweringStorage& { return lowering; }
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
    auto if_stmt(Expr condition,
                 BlockId then_block,
                 std::optional<BlockId> else_block = {},
                 bool is_constexpr = false) -> NodeId;
    // Require a variable declaration, assignment or expression statement as the initializer.
    auto for_stmt(NodeId initializer, Expr condition, Expr increment, BlockId body) -> NodeId;
    auto switch_case(std::optional<Expr> label, BlockId body, bool braced = true) -> SwitchCaseId;
    auto switch_stmt(Expr condition, std::span<SwitchCaseId const> cases) -> NodeId;
    auto switch_stmt(Expr condition, std::initializer_list<SwitchCaseId> cases) -> NodeId {
        return switch_stmt(condition, std::span<SwitchCaseId const>{cases});
    }
    auto break_stmt() -> NodeId;
    // Annotate the following declaration without introducing bindings into its scope.
    auto declaration_macro(Syntax invocation) -> NodeId;
    auto macro_invocation(Syntax name, std::span<Syntax const> arguments = {}) -> Syntax;
    auto macro_invocation(Syntax name, std::initializer_list<Syntax> arguments) -> Syntax {
        return macro_invocation(name, std::span<Syntax const>{arguments});
    }
    auto macro_statement(Syntax name, std::span<Syntax const> arguments = {}) -> NodeId;
    auto macro_statement(Syntax name, std::initializer_list<Syntax> arguments) -> NodeId {
        return macro_statement(name, std::span<Syntax const>{arguments});
    }
    auto macro_block(Syntax invocation, BlockId body) -> NodeId;
    auto binding_list(std::initializer_list<std::string_view> names) -> Syntax;
    auto expr_stmt(Expr expr, std::span<TypeDependencyId const> dependencies = {}) -> NodeId;
    auto expr_stmt(Expr expr, std::initializer_list<TypeDependencyId> dependencies) -> NodeId {
        return expr_stmt(expr, std::span<TypeDependencyId const>{dependencies});
    }
    auto return_stmt(std::optional<Syntax> expr = {},
                     std::span<TypeDependencyId const> dependencies = {}) -> NodeId;
    auto return_stmt(std::optional<Syntax> expr,
                     std::initializer_list<TypeDependencyId> dependencies) -> NodeId {
        return return_stmt(expr, std::span<TypeDependencyId const>{dependencies});
    }
    auto assignment(Expr target, Syntax value, std::span<TypeDependencyId const> dependencies = {})
        -> NodeId;
    auto assignment(Expr target, Syntax value, std::initializer_list<TypeDependencyId> dependencies)
        -> NodeId {
        return assignment(target, value, std::span<TypeDependencyId const>{dependencies});
    }
    auto variable(CppTypeId type,
                  Syntax name,
                  std::optional<Syntax> initializer = {},
                  VariableOptions options = {}) -> NodeId;
    auto variable(CppTypeId type, StringId name, std::optional<Syntax> initializer = {}) -> NodeId;
    auto variable(CppTypeId type, std::string_view name, std::optional<Syntax> initializer = {})
        -> NodeId;
    // Text type overloads intern spellings without dependencies; use CppTypeId for resolved types.
    auto variable(std::string_view type,
                  std::string_view name,
                  std::optional<Syntax> initializer = {}) -> NodeId;
    auto static_assertion(StringId condition, StringId message) -> NodeId;
    auto static_assertion(Expr condition, StringId message = {}) -> NodeId;
    auto assertion(Expr condition) -> NodeId;
    auto access(Access value = Access::public_,
                AccessIndentation indentation = AccessIndentation::outdented) -> NodeId;
    auto include(StringId path, IncludeStyle style = IncludeStyle::inferred) -> NodeId;
    auto include_dependencies() -> NodeId;
    auto forward_declaration(StringId name,
                             RecordKind kind = RecordKind::struct_,
                             TemplateId declaration = {},
                             StringId export_specifier = {}) -> NodeId;
    auto friend_declaration(StringId name,
                            RecordKind kind = RecordKind::class_,
                            TemplateId declaration = {}) -> NodeId;
    auto friend_type(CppTypeId type) -> NodeId;
    auto using_declaration(StringId name, CppTypeId type, TemplateId declaration = {}) -> NodeId;
    auto using_import(CppTypeId qualified_name) -> NodeId;
    auto instance_member(CppTypeId type, StringId name, std::optional<Syntax> initializer = {})
        -> NodeId;
    auto static_member(CppTypeId type,
                       StringId name,
                       std::optional<Syntax> initializer = {},
                       bool is_inline = false,
                       TemplateId template_declaration = {}) -> NodeId;
    // Require a nonempty initializer expression.
    auto constexpr_member(CppTypeId type,
                          StringId name,
                          Syntax initializer,
                          TemplateId template_declaration = {}) -> NodeId;
    auto enumerator(StringId name,
                    std::optional<Syntax> initializer = {},
                    std::optional<StringId> annotation = {}) -> EnumValueId;
    auto enumeration(StringId name,
                     CppTypeId underlying_type,
                     std::span<EnumValueId const> values,
                     StringId export_specifier = {},
                     bool scoped = true) -> NodeId;
    auto enumeration(StringId name,
                     CppTypeId underlying_type,
                     std::initializer_list<EnumValueId> values,
                     StringId export_specifier = {},
                     bool scoped = true) -> NodeId {
        return enumeration(
            name, underlying_type, std::span<EnumValueId const>{values}, export_specifier, scoped);
    }
    auto add_record(RecordDefinition const& definition) -> RecordId;
    auto namespace_node(StringId name, std::span<NodeId const> children) -> NodeId;
    auto namespace_node(StringId name, std::initializer_list<NodeId> children) -> NodeId {
        return namespace_node(name, std::span<NodeId const>{children});
    }
    auto type_template_parameter(StringId name, CppTypeId default_type = {}, bool pack = false)
        -> TemplateParameterId;
    auto value_template_parameter(CppTypeId type,
                                  StringId name = {},
                                  Syntax default_value = {},
                                  bool pack = false) -> TemplateParameterId;
    auto template_declaration(std::span<TemplateParameterId const> parameters) -> TemplateId;
    auto template_declaration(std::initializer_list<TemplateParameterId> parameters) -> TemplateId {
        return template_declaration(std::span<TemplateParameterId const>{parameters});
    }
    auto template_specialization() -> TemplateId;
    // Retain arbitrary parameter-list syntax authored by the source language.
    auto raw_template_declaration(StringId parameters) -> TemplateId;
    // Require a return type for ordinary functions; constructors and destructors omit it.
    auto add_function(FunctionDefinition const& definition) -> FunctionId;
    template <typename... Definitions>
        requires (std::same_as<std::remove_cvref_t<Definitions>, FunctionDefinition> && ...)
    auto add_functions(Definitions const&... definitions)
        -> std::array<FunctionId, sizeof...(Definitions)> {
        return {add_function(definitions)...};
    }
    // Views borrow storage; do not retain them across IR mutations.
    auto get_view(RecordId record) const -> RecordView;
    auto get_view(FunctionId function) const -> FunctionView;
    auto get_view(FunctionParameterId parameter) const -> FunctionParameterView;
    auto parameter(CppTypeId type, StringId name = {}, Syntax default_value = {})
        -> FunctionParameterId;
    auto parameter(CppTypeId type, std::string_view name, Syntax default_value = {})
        -> FunctionParameterId;
    auto parameter(std::string_view type, std::string_view name = {}, Syntax default_value = {})
        -> FunctionParameterId;
    auto member_initializer(StringId member, StringId value) -> MemberInitializerId;
    auto member_initializer(StringId member, Syntax value) -> MemberInitializerId;
    auto raw(StringId value, std::span<TypeDependencyId const> dependencies = {}) -> NodeId;
    // Interleave authored text fragments with structured substitutions at final emission.
    auto raw_parts(std::span<Syntax const> parts,
                   std::span<TypeDependencyId const> dependencies = {}) -> NodeId;
    auto raw_lines(std::span<StringId const> lines,
                   std::span<TypeDependencyId const> dependencies = {}) -> NodeId;
    auto raw(std::string_view value, std::span<TypeDependencyId const> dependencies = {}) -> NodeId;
    auto raw(std::string_view value, std::initializer_list<TypeDependencyId> dependencies)
        -> NodeId {
        return raw(value, std::span<TypeDependencyId const>{dependencies});
    }
    // Require a positive formatting count.
    auto lines(int count) -> NodeId;
    auto declaration(FunctionId function) -> NodeId;
    auto header_function(FunctionId function) -> NodeId;
    auto definition(FunctionId function,
                    std::optional<CppTypeId> owner = {},
                    std::optional<ReturnSyntax> syntax = {},
                    bool header = false) -> NodeId;
    // Refer to a local name. Use type_reference with structured CppTypes for qualified symbols.
    auto name(std::string_view value, std::span<TypeDependencyId const> dependencies = {})
        -> Syntax;
    auto name(StringId value, std::span<TypeDependencyId const> dependencies = {}) -> Syntax;
    auto name(std::string_view value, std::initializer_list<TypeDependencyId> dependencies)
        -> Syntax {
        return name(value, std::span<TypeDependencyId const>{dependencies});
    }
    auto reference(Syntax symbol, CppTypeId type) -> Expr;
    auto dependent_reference(Syntax symbol) -> Expr;
    // Return the concrete or symbolic C++ type of a value expression.
    auto expression_type(Expr expression) const -> CppTypeId;
    auto authored_fragment(std::string_view value) -> Syntax;
    auto authored_fragment(StringId value) -> Syntax;
    auto named(std::string_view value, std::span<TypeDependencyId const> dependencies = {}) -> Expr;
    auto named(StringId value, std::span<TypeDependencyId const> dependencies = {}) -> Expr;
    auto named(std::string_view value, std::initializer_list<TypeDependencyId> dependencies)
        -> Expr {
        return named(value, std::span<TypeDependencyId const>{dependencies});
    }
    auto literal(auto&& value) -> Expr {
        using Value = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::same_as<Value, bool>) {
            return boolean_literal(value);
        } else if constexpr (std::same_as<Value, std::nullptr_t>) {
            return nullptr_literal();
        } else if constexpr (ir::NumericLiteralValue<Value>) {
            return numeric_literal(value);
        } else {
            static_assert(false, "Unsupported literal type");
        }
    }
    // Require a C++ literal spelling with exactly the supplied type.
    auto literal(CppTypeId type, std::string_view spelling) -> Expr;
    auto string_literal(std::string_view value) -> Expr;
    auto string_literal(StringId value) -> Expr;
    auto raw_expr(CppTypeId type,
                  StringId value,
                  std::span<TypeDependencyId const> dependencies = {}) -> Expr;
    auto raw_expr(CppTypeId type,
                  std::string_view value,
                  std::span<TypeDependencyId const> dependencies = {}) -> Expr;
    auto raw_expr(CppTypeId type,
                  std::string_view value,
                  std::initializer_list<TypeDependencyId> dependencies) -> Expr {
        return raw_expr(type, value, std::span<TypeDependencyId const>{dependencies});
    }
    auto call(Syntax callee, std::span<Syntax const> arguments = {}) -> Expr;
    auto call(CppTypeId callee, std::span<Syntax const> arguments = {}) -> Expr {
        return call(type_reference(callee), arguments);
    }
    auto call(CppTypeId callee, std::initializer_list<Syntax> arguments) -> Expr {
        return call(callee, std::span<Syntax const>{arguments});
    }
    auto template_call(Syntax callee,
                       std::span<Syntax const> template_arguments,
                       std::span<Syntax const> arguments = {}) -> Expr {
        return call(template_id(callee, template_arguments), arguments);
    }
    auto template_call(Syntax callee,
                       std::initializer_list<Syntax> template_arguments,
                       std::span<Syntax const> arguments = {}) -> Expr {
        return template_call(callee, std::span<Syntax const>{template_arguments}, arguments);
    }
    auto template_call(Syntax callee,
                       std::span<Syntax const> template_arguments,
                       std::initializer_list<Syntax> arguments) -> Expr {
        return template_call(callee, template_arguments, std::span<Syntax const>{arguments});
    }
    auto template_call(Syntax callee,
                       std::initializer_list<Syntax> template_arguments,
                       std::initializer_list<Syntax> arguments) -> Expr {
        return template_call(callee,
                             std::span<Syntax const>{template_arguments},
                             std::span<Syntax const>{arguments});
    }
    // Capture local names by value or apply address_of for explicit reference captures.
    auto lambda(std::span<Syntax const> captures,
                std::span<FunctionParameterId const> parameters,
                BlockId body,
                TemplateId template_declaration = {}) -> Expr;
    auto lambda(std::initializer_list<Syntax> captures,
                std::initializer_list<FunctionParameterId> parameters,
                BlockId body,
                TemplateId template_declaration = {}) -> Expr {
        return lambda(std::span<Syntax const>{captures},
                      std::span<FunctionParameterId const>{parameters},
                      body,
                      template_declaration);
    }
    auto call(Syntax callee, std::initializer_list<Syntax> arguments) -> Expr {
        return call(callee, std::span<Syntax const>{arguments});
    }
    auto member_access(Expr object, std::string_view member) -> Expr;
    auto member_access(Expr object, StringId member) -> Expr;
    auto member_name(Expr object, StringId member, bool through_pointer = false) -> Syntax;
    auto member_name(Expr object, std::string_view member, bool through_pointer = false) -> Syntax;
    auto scope_name(CppTypeId owner, StringId member) -> Syntax;
    auto scope_name(CppTypeId owner, std::string_view member) -> Syntax;
    auto scope_member(CppTypeId owner, std::string_view member) -> Expr;
    auto scope_member(CppTypeId owner, StringId member) -> Expr;
    auto pointer_member_access(Expr object, std::string_view member) -> Expr;
    auto pointer_member_access(Expr object, StringId member) -> Expr;
    auto this_member(std::string_view member) -> Expr {
        return pointer_member_access(named("this"), member);
    }
    auto member_call(Expr object, std::string_view member, std::span<Syntax const> arguments = {})
        -> Expr {
        return call(member_name(object, member), arguments);
    }
    auto member_call(Expr object, std::string_view member, std::initializer_list<Syntax> arguments)
        -> Expr {
        return member_call(object, member, std::span<Syntax const>{arguments});
    }
    auto member_call(Expr object, StringId member, std::span<Syntax const> arguments = {}) -> Expr {
        return call(member_name(object, member), arguments);
    }
    auto member_call(Expr object, StringId member, std::initializer_list<Syntax> arguments)
        -> Expr {
        return member_call(object, member, std::span<Syntax const>{arguments});
    }
    auto pointer_member_call(Expr object,
                             std::string_view member,
                             std::span<Syntax const> arguments = {}) -> Expr {
        return call(member_name(object, member, true), arguments);
    }
    auto pointer_member_call(Expr object,
                             std::string_view member,
                             std::initializer_list<Syntax> arguments) -> Expr {
        return pointer_member_call(object, member, std::span<Syntax const>{arguments});
    }
    auto pointer_member_call(Expr object, StringId member, std::span<Syntax const> arguments = {})
        -> Expr {
        return call(member_name(object, member, true), arguments);
    }
    auto pointer_member_call(Expr object, StringId member, std::initializer_list<Syntax> arguments)
        -> Expr {
        return pointer_member_call(object, member, std::span<Syntax const>{arguments});
    }
    auto this_member_call(std::string_view member, std::span<Syntax const> arguments = {}) -> Expr {
        return pointer_member_call(named("this"), member, arguments);
    }
    auto this_member_call(std::string_view member, std::initializer_list<Syntax> arguments)
        -> Expr {
        return this_member_call(member, std::span<Syntax const>{arguments});
    }
    auto this_member_call(StringId member, std::span<Syntax const> arguments = {}) -> Expr {
        return pointer_member_call(named("this"), member, arguments);
    }
    auto this_member_call(StringId member, std::initializer_list<Syntax> arguments) -> Expr {
        return this_member_call(member, std::span<Syntax const>{arguments});
    }
    auto pack_expansion(Syntax pattern) -> Syntax;
    auto capture(std::string_view name, bool by_reference = false) -> Syntax;
    auto unary(UnaryOperator operation, Expr operand) -> Expr;
    auto address_of(Syntax symbol) -> Expr;
    auto subscript(Expr object, Expr index) -> Expr;
    auto binary(BinaryOperator operation, Expr left, Expr right, bool line_break_after = false)
        -> Expr;
    auto type_reference(CppTypeId type) -> Syntax;
    auto decltype_type(Expr operand) -> CppTypeId;
    auto template_type(CppTypeId primary, std::span<Syntax const> arguments) -> CppTypeId;
    auto template_type(CppTypeId primary, std::initializer_list<Syntax> arguments) -> CppTypeId {
        return template_type(primary, std::span<Syntax const>{arguments});
    }
    auto parenthesized(Expr expr) -> Expr;
    auto template_id(Syntax callee, std::span<Syntax const> arguments) -> Syntax;
    auto template_id(Syntax callee, std::initializer_list<Syntax> arguments) -> Syntax {
        return template_id(callee, std::span<Syntax const>{arguments});
    }
    auto static_cast_expr(CppTypeId type, Expr operand) -> Expr;
    auto reinterpret_cast_expr(CppTypeId type, Expr operand) -> Expr;
    auto conditional(Expr condition, Expr when_true, Expr when_false) -> Expr;
    auto sizeof_type(CppTypeId type) -> Expr;
    auto init_list(std::span<Syntax const> elements) -> Syntax;
    auto init_list(std::span<Syntax const> elements, CppTypeId type) -> Expr;
    // Use designated initializers only as initializer-list elements.
    auto designated_initializer(StringId member, Syntax value) -> Syntax;
    auto init_list(std::initializer_list<Syntax> elements, CppTypeId type) -> Expr {
        return init_list(std::span<Syntax const>{elements}, type);
    }
    auto init_list(std::initializer_list<Syntax> elements) -> Syntax {
        return init_list(std::span<Syntax const>{elements});
    }
    void collect_dependencies(Syntax syntax, std::vector<TypeDependencyId>& result) const;
    void collect_dependencies(Expr expr, std::vector<TypeDependencyId>& result) const;
    void collect_dependencies(NodeId node, std::vector<TypeDependencyId>& result) const;
    void for_each_child(NodeId node, std::function<void(NodeId)> const& visit) const;
    // Describe all bindings introduced by raw content or inherited bases in this scope.
    // Structured declarations are collected automatically in addition to these names.
    void set_scope_bindings(NodeId node, std::span<StringId const> names);
    // Replace file roots with normalized nodes; existing nodes and handles remain valid.
    void merge_adjacent_namespaces(CppFileId file);
  private:
    friend class CppRenderer;
    auto finish_expression(Expr expression, CppTypeId type) -> Expr;
    auto dependent_expression(Expr expression) -> Expr;
    auto boolean_literal(bool value) -> Expr;
    auto nullptr_literal() -> Expr;
    template <ir::NumericLiteralValue Value>
    auto numeric_literal(Value number) -> Expr;
    auto merge_adjacent_namespaces(std::vector<NodeId>& values) -> bool;
    void collect_dependencies(TemplateId declaration, std::vector<TypeDependencyId>& result) const;
    ir::TypeTemplateParameters type_template_parameters_;
    ir::ValueTemplateParameters value_template_parameters_;
    ir::Templates templates_;
    std::vector<TemplateParameterId> template_parameter_references_;
    ir::TemplateIdExprs template_id_exprs_;
    ir::ParenthesizedExprs parenthesized_exprs_;
    auto function_node(FunctionId function,
                       std::optional<CppTypeId> owner,
                       bool declaration,
                       bool header,
                       std::optional<ReturnSyntax> return_syntax) -> NodeId;
    auto save_nodes(std::span<NodeId const> values) -> ir::Range;
    auto save_exprs(std::span<Syntax const> values) -> ir::Range;
    auto save_types(std::span<CppTypeId const> values) -> ir::Range;
    auto save_dependencies(std::span<TypeDependencyId const> values) -> ir::Range;
    auto nodes(ir::Range range) const -> std::span<NodeId const>;
    auto exprs(ir::Range range) const -> std::span<Syntax const>;
    auto type_list(ir::Range range) const -> std::span<CppTypeId const>;
    auto dependency_list(ir::Range range) const -> std::span<TypeDependencyId const>;
    std::unordered_map<std::uint32_t, std::vector<StringId>> scope_bindings_;
    std::vector<FunctionParameterId> parameter_references_;
    std::vector<MemberInitializerId> initializer_references_;
    std::vector<SwitchCaseId> case_references_;
    std::vector<EnumValueId> enumerator_references_;
    std::vector<NodeId> children_;
    std::vector<Syntax> arguments_;
    ioj::EnumArray<std::vector<CppTypeId>, ExprKind> expression_types_;
    std::vector<Syntax> symbol_references_;
    std::vector<Syntax> pack_patterns_;
    std::vector<StringId> authored_fragments_;
    std::vector<CppTypeId> scope_name_owners_;
    std::vector<StringId> scope_name_members_;
    std::vector<StringId> binding_names_;
    std::vector<ir::Range> binding_lists_;
    std::vector<CppTypeId> type_references_;
    std::vector<TypeDependencyId> dependencies_;
    ir::RawNodes raw_nodes_;
    ir::RawLines raw_lines_nodes_;
    std::vector<StringId> raw_lines_;
    ir::LineComments line_comment_nodes_;
    ir::LineDirectives line_directive_nodes_;
    ir::BlockComments block_comment_nodes_;
    ir::Blocks block_nodes_;
    ir::IfStmts if_stmt_nodes_;
    ir::ForStmts for_stmt_nodes_;
    ir::Switches switch_stmt_nodes_;
    ir::ExprStmts expr_stmt_nodes_;
    ir::DeclarationMacros declaration_macro_nodes_;
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
    ir::Names named_exprs_;
    ir::Literals literal_exprs_;
    ir::NumericLiterals numeric_literal_exprs_;
    std::vector<StringId> string_literal_exprs_;
    ir::Calls call_exprs_;
    ir::MemberExprs member_exprs_;
    ir::Subscripts subscript_exprs_;
    ir::BinaryExprs binary_exprs_;
    ir::UnaryExprs unary_exprs_;
    ir::Casts cast_exprs_;
    ir::ConditionalExprs conditional_exprs_;
    ir::LambdaExprs lambda_exprs_;
    ir::SizeofExprs sizeof_exprs_;
    ir::InitializerExprs initializer_exprs_;
    ir::DesignatedInitializers designated_initializers_;
    ir::RawExprs raw_exprs_;
    ir::Parameters parameters_;
    ir::MemberInitializers initializers_;
    ir::Enumerators enumerators_;
    ir::SwitchCases cases_;
    ir::Functions functions_;
    ir::FunctionBodies function_bodies_;
    std::vector<Expr> conditional_noexcept_;
};

extern template auto CppIR::numeric_literal(std::int8_t) -> Expr;
extern template auto CppIR::numeric_literal(std::int16_t) -> Expr;
extern template auto CppIR::numeric_literal(std::int32_t) -> Expr;
extern template auto CppIR::numeric_literal(std::int64_t) -> Expr;
extern template auto CppIR::numeric_literal(std::uint8_t) -> Expr;
extern template auto CppIR::numeric_literal(std::uint16_t) -> Expr;
extern template auto CppIR::numeric_literal(std::uint32_t) -> Expr;
extern template auto CppIR::numeric_literal(std::uint64_t) -> Expr;
extern template auto CppIR::numeric_literal(float) -> Expr;
extern template auto CppIR::numeric_literal(double) -> Expr;

} // namespace lispb::codegen
