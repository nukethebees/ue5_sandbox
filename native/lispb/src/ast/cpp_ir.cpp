#include <codegen/ast/cpp_ir.h>

#include <ioj/is_any.h>

#include <codegen/ast/access.h>
#include <codegen/ast/access_indentation.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/expr_kind.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_disposition_placement.h>
#include <codegen/ast/function_exception_id.h>
#include <codegen/ast/function_exception_kind.h>
#include <codegen/ast/function_implementation_id.h>
#include <codegen/ast/function_implementation_kind.h>
#include <codegen/ast/function_kind.h>
#include <codegen/ast/function_opening_brace_placement.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/function_requires_placement.h>
#include <codegen/ast/function_template_placement.h>
#include <codegen/ast/function_trailing_return_placement.h>
#include <codegen/ast/include.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/ir/range_storage.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_kind.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/template_kind.h>
#include <codegen/ast/template_parameter_kind.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/cpp_string.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {

namespace ir_detail {
auto node(NodeKind kind, std::size_t index) -> NodeId {
    assert(index <= NodeId::index_mask);
    return NodeId{(static_cast<std::uint32_t>(kind) << NodeId::kind_shift) |
                  static_cast<std::uint32_t>(index)};
}
}

namespace expr_detail {
auto handle(ExprKind kind, std::size_t index) -> Expr {
    assert(index <= Expr::index_mask);
    return Expr{(static_cast<std::uint32_t>(kind) << Expr::kind_shift) |
                static_cast<std::uint32_t>(index)};
}
}

/* **************************************** */
// Construction and storage
/* **************************************** */
auto CppIR::store_text(std::string_view value) -> StringId {
    return strings.intern(value);
}
auto CppIR::store_optional_text(std::optional<std::string_view> value) -> std::optional<StringId> {
    return value ? std::optional{strings.intern(*value)} : std::nullopt;
}

auto CppIR::save_nodes(std::span<NodeId const> values) -> ir::Range {
    return ir::save_range(children_, values);
}
auto CppIR::nodes(ir::Range range) const -> std::span<NodeId const> {
    return ir::range_values(children_, range);
}
auto CppIR::save_exprs(std::span<Syntax const> values) -> ir::Range {
    return ir::save_range(arguments_, values);
}
auto CppIR::exprs(ir::Range range) const -> std::span<Syntax const> {
    return ir::range_values(arguments_, range);
}
auto CppIR::save_types(std::span<CppTypeId const> values) -> ir::Range {
    return ir::save_range(type_references_, values);
}
auto CppIR::type_list(ir::Range range) const -> std::span<CppTypeId const> {
    return ir::range_values(type_references_, range);
}
auto CppIR::save_dependencies(std::span<TypeDependencyId const> values) -> ir::Range {
    return ir::save_range(dependencies_, values);
}
auto CppIR::dependency_list(ir::Range range) const -> std::span<TypeDependencyId const> {
    return ir::range_values(dependencies_, range);
}
auto CppIR::line_comment(StringId text) -> NodeId {
    auto const index{line_comment_nodes_.text.size()};
    line_comment_nodes_.text.push_back(text);
    return ir_detail::node(NodeKind::line_comment, index);
}

auto CppIR::block_comment(StringId text) -> NodeId {
    auto const index{block_comment_nodes_.text.size()};
    block_comment_nodes_.text.push_back(text);
    return ir_detail::node(NodeKind::block_comment, index);
}

auto CppIR::line_comment(std::string_view text) -> NodeId {
    return line_comment(strings.intern(text));
}

auto CppIR::block_comment(std::string_view text) -> NodeId {
    return block_comment(strings.intern(text));
}

auto CppIR::line_directive(std::uint32_t line, std::string_view path) -> NodeId {
    auto const index{line_directive_nodes_.line.size()};
    line_directive_nodes_.line.push_back(line);
    line_directive_nodes_.path.push_back(strings.intern(path));
    return ir_detail::node(NodeKind::line_directive, index);
}

auto CppIR::block(std::span<NodeId const> children) -> BlockId {
    auto const index{block_nodes_.children.size()};
    block_nodes_.children.push_back(save_nodes(children));
    return BlockId{ir_detail::node(NodeKind::block, index).value};
}

auto CppIR::if_stmt(Expr condition,
                    BlockId then_block,
                    std::optional<BlockId> else_block,
                    bool is_constexpr) -> NodeId {
    auto const index{if_stmt_nodes_.condition.size()};
    if_stmt_nodes_.condition.push_back(condition);
    if_stmt_nodes_.then_children.push_back(block_nodes_.children[then_block.index()]);
    if_stmt_nodes_.else_children.push_back(
        else_block ? std::optional{block_nodes_.children[else_block->index()]} : std::nullopt);
    if_stmt_nodes_.is_constexpr.push_back(is_constexpr);
    return ir_detail::node(NodeKind::if_stmt, index);
}

auto CppIR::switch_case(std::optional<Expr> label, BlockId body, bool braced) -> SwitchCaseId {
    SwitchCaseId const result{static_cast<std::uint32_t>(cases_.label.size())};
    cases_.label.push_back(label);
    cases_.body.push_back(block_nodes_.children[body.index()]);
    cases_.braced.push_back(braced);
    return result;
}

auto CppIR::for_stmt(NodeId initializer, Expr condition, Expr increment, BlockId body) -> NodeId {
    assert(ioj::is_any(
        initializer.kind(),
        {NodeKind::variable_declaration_stmt, NodeKind::assignment_stmt, NodeKind::expr_stmt}));
    auto const index{for_stmt_nodes_.initializer.size()};
    for_stmt_nodes_.initializer.push_back(initializer);
    for_stmt_nodes_.condition.push_back(condition);
    for_stmt_nodes_.increment.push_back(increment);
    for_stmt_nodes_.children.push_back(block_nodes_.children[body.index()]);
    return ir_detail::node(NodeKind::for_stmt, index);
}

auto CppIR::switch_stmt(Expr condition, std::span<SwitchCaseId const> cases) -> NodeId {
    auto const index{switch_stmt_nodes_.condition.size()};
    switch_stmt_nodes_.condition.push_back(condition);
    switch_stmt_nodes_.cases.push_back(ir::save_range(case_references_, cases));
    return ir_detail::node(NodeKind::switch_stmt, index);
}

auto CppIR::break_stmt() -> NodeId {
    return ir_detail::node(NodeKind::break_stmt, 0);
}

auto CppIR::expr_stmt(Expr expr, std::span<TypeDependencyId const> dependencies) -> NodeId {
    auto const index{expr_stmt_nodes_.expr.size()};
    expr_stmt_nodes_.expr.push_back(expr);
    expr_stmt_nodes_.dependencies.push_back(save_dependencies(dependencies));
    return ir_detail::node(NodeKind::expr_stmt, index);
}

auto CppIR::declaration_macro(Syntax invocation) -> NodeId {
    assert(invocation.kind() == SyntaxKind::macro_invocation);
    auto const index{declaration_macro_nodes_.invocation.size()};
    declaration_macro_nodes_.invocation.push_back(invocation);
    declaration_macro_nodes_.body.push_back({});
    declaration_macro_nodes_.statement.push_back(false);
    return ir_detail::node(NodeKind::declaration_macro, index);
}

auto CppIR::return_stmt(std::optional<Syntax> expr, std::span<TypeDependencyId const> dependencies)
    -> NodeId {
    auto const index{return_stmt_nodes_.expr.size()};
    return_stmt_nodes_.expr.push_back(expr);
    return_stmt_nodes_.dependencies.push_back(save_dependencies(dependencies));
    return ir_detail::node(NodeKind::return_stmt, index);
}

auto CppIR::assignment(Expr target, Syntax value, std::span<TypeDependencyId const> dependencies)
    -> NodeId {
    auto const index{assignment_stmt_nodes_.target.size()};
    assignment_stmt_nodes_.target.push_back(target);
    assignment_stmt_nodes_.value.push_back(value);
    assignment_stmt_nodes_.dependencies.push_back(save_dependencies(dependencies));
    return ir_detail::node(NodeKind::assignment_stmt, index);
}

auto CppIR::variable(CppTypeId type,
                     Syntax name,
                     std::optional<Syntax> initializer,
                     VariableOptions const options) -> NodeId {
    auto const index{variable_declaration_stmt_nodes_.type.size()};
    variable_declaration_stmt_nodes_.type.push_back(type);
    variable_declaration_stmt_nodes_.name.push_back(name);
    variable_declaration_stmt_nodes_.initializer.push_back(initializer);
    variable_declaration_stmt_nodes_.template_declaration.push_back(options.template_declaration);
    variable_declaration_stmt_nodes_.is_inline.push_back(options.is_inline);
    variable_declaration_stmt_nodes_.is_constexpr.push_back(options.is_constexpr);
    return ir_detail::node(NodeKind::variable_declaration_stmt, index);
}

auto CppIR::variable(CppTypeId type, StringId name, std::optional<Syntax> initializer) -> NodeId {
    return variable(type, this->name(name), initializer);
}

auto CppIR::variable(CppTypeId type, std::string_view name, std::optional<Syntax> initializer)
    -> NodeId {
    return variable(type, strings.intern(name), initializer);
}

auto CppIR::variable(std::string_view type,
                     std::string_view name,
                     std::optional<Syntax> initializer) -> NodeId {
    return variable(types.intern(type), strings.intern(name), initializer);
}

auto CppIR::static_assertion(StringId condition, StringId message) -> NodeId {
    return static_assertion(raw_expr(types.std.bool_, condition), message);
}

auto CppIR::static_assertion(Expr condition, StringId message) -> NodeId {
    auto const index{static_assert_nodes_.condition.size()};
    static_assert_nodes_.condition.push_back(condition);
    static_assert_nodes_.message.push_back(message);
    return ir_detail::node(NodeKind::static_assert_, index);
}

auto CppIR::access(Access value, AccessIndentation indentation) -> NodeId {
    auto const index{access_specifier_nodes_.access.size()};
    access_specifier_nodes_.access.push_back(value);
    access_specifier_nodes_.indentation.push_back(indentation);
    return ir_detail::node(NodeKind::access_specifier, index);
}

auto CppIR::include(StringId path, IncludeStyle style) -> NodeId {
    auto const index{include_nodes_.path.size()};
    include_nodes_.path.push_back(path);
    include_nodes_.style.push_back(style);
    return ir_detail::node(NodeKind::include, index);
}

auto CppIR::assertion(Expr condition) -> NodeId {
    return macro_statement(name("assert", {types.dependency("assert", "cassert")}),
                           {parenthesized(condition)});
}

auto CppIR::include_dependencies() -> NodeId {
    return ir_detail::node(NodeKind::include_dependencies, 0);
}

auto CppIR::forward_declaration(StringId name,
                                RecordKind kind,
                                TemplateId declaration,
                                StringId export_specifier) -> NodeId {
    auto const index{forward_declaration_nodes_.name.size()};
    forward_declaration_nodes_.name.push_back(name);
    forward_declaration_nodes_.kind.push_back(kind);
    forward_declaration_nodes_.template_declaration.push_back(declaration);
    forward_declaration_nodes_.export_specifier.push_back(export_specifier);
    return ir_detail::node(NodeKind::forward_declaration, index);
}

auto CppIR::friend_declaration(StringId name, RecordKind kind, TemplateId declaration) -> NodeId {
    auto const index{friend_declaration_nodes_.name.size()};
    friend_declaration_nodes_.name.push_back(name);
    friend_declaration_nodes_.kind.push_back(kind);
    friend_declaration_nodes_.template_declaration.push_back(declaration);
    friend_declaration_nodes_.type.push_back({});
    return ir_detail::node(NodeKind::friend_declaration, index);
}

auto CppIR::friend_type(CppTypeId type) -> NodeId {
    auto const index{friend_declaration_nodes_.name.size()};
    friend_declaration_nodes_.name.push_back({});
    friend_declaration_nodes_.kind.push_back(RecordKind::class_);
    friend_declaration_nodes_.template_declaration.push_back({});
    friend_declaration_nodes_.type.push_back(type);
    return ir_detail::node(NodeKind::friend_declaration, index);
}

auto CppIR::using_declaration(StringId name, CppTypeId type, TemplateId declaration) -> NodeId {
    auto const index{using_declaration_nodes_.name.size()};
    using_declaration_nodes_.name.push_back(name);
    using_declaration_nodes_.type.push_back(type);
    using_declaration_nodes_.template_declaration.push_back(declaration);
    return ir_detail::node(NodeKind::using_declaration, index);
}

auto CppIR::using_import(CppTypeId qualified_name) -> NodeId {
    return using_declaration({}, types.reference(qualified_name));
}

auto CppIR::instance_member(CppTypeId type, StringId name, std::optional<Syntax> initializer)
    -> NodeId {
    auto const index{instance_members_.name.size()};
    instance_members_.type.push_back(type);
    instance_members_.name.push_back(name);
    instance_members_.initializer.push_back(initializer.value_or(Syntax{}));
    return ir_detail::node(NodeKind::instance_member, index);
}

auto CppIR::static_member(CppTypeId type,
                          StringId name,
                          std::optional<Syntax> initializer,
                          bool is_inline,
                          TemplateId template_declaration) -> NodeId {
    auto const index{static_members_.name.size()};
    static_members_.type.push_back(type);
    static_members_.name.push_back(name);
    static_members_.initializer.push_back(initializer.value_or(Syntax{}));
    static_members_.template_declaration.push_back(template_declaration);
    static_members_.is_inline.push_back(is_inline);
    return ir_detail::node(NodeKind::static_member, index);
}

auto CppIR::constexpr_member(CppTypeId type,
                             StringId name,
                             Syntax initializer,
                             TemplateId template_declaration) -> NodeId {
    assert(initializer);
    auto const index{constexpr_members_.name.size()};
    constexpr_members_.type.push_back(type);
    constexpr_members_.name.push_back(name);
    constexpr_members_.initializer.push_back(initializer);
    constexpr_members_.template_declaration.push_back(template_declaration);
    return ir_detail::node(NodeKind::constexpr_member, index);
}

auto CppIR::enumerator(StringId name,
                       std::optional<Syntax> initializer,
                       std::optional<StringId> annotation) -> EnumValueId {
    EnumValueId const result{static_cast<std::uint32_t>(enumerators_.name.size())};
    enumerators_.name.push_back(name);
    enumerators_.initializer.push_back(initializer);
    enumerators_.annotation.push_back(annotation);
    return result;
}

auto CppIR::enumeration(StringId name,
                        CppTypeId underlying_type,
                        std::span<EnumValueId const> values,
                        StringId export_specifier,
                        bool scoped) -> NodeId {
    auto const index{enum_nodes_.name.size()};
    enum_nodes_.name.push_back(name);
    enum_nodes_.underlying_type.push_back(underlying_type);
    enum_nodes_.values.push_back(ir::save_range(enumerator_references_, values));
    enum_nodes_.export_specifier.push_back(export_specifier);
    enum_nodes_.scoped.push_back(scoped);
    return ir_detail::node(NodeKind::enum_, index);
}

auto CppIR::add_record(RecordDefinition const& definition) -> RecordId {
    auto const index{struct_nodes_.name.size()};
    struct_nodes_.name.push_back(definition.name);
    struct_nodes_.children.push_back(save_nodes(definition.children));
    struct_nodes_.bases.push_back(save_types(definition.bases));
    struct_nodes_.template_declaration.push_back(definition.template_declaration);
    struct_nodes_.requires_clause.push_back(definition.requires_clause);
    struct_nodes_.export_specifier.push_back(definition.export_specifier);
    struct_nodes_.record_kind.push_back(definition.kind);
    struct_nodes_.dependencies.push_back(save_dependencies(definition.dependencies));
    auto const result{RecordId{ir_detail::node(NodeKind::struct_, index).value}};
    if (definition.scope_bindings) {
        set_scope_bindings(result, *definition.scope_bindings);
    }
    return result;
}

auto CppIR::get_view(RecordId record) const -> RecordView {
    auto const index{record.index()};
    auto const bindings{scope_bindings_.find(static_cast<NodeId>(record).value)};
    return {.name = struct_nodes_.name[index],
            .kind = struct_nodes_.record_kind[index],
            .children = nodes(struct_nodes_.children[index]),
            .bases = type_list(struct_nodes_.bases[index]),
            .template_declaration = struct_nodes_.template_declaration[index],
            .requires_clause = struct_nodes_.requires_clause[index],
            .export_specifier = struct_nodes_.export_specifier[index],
            .dependencies = dependency_list(struct_nodes_.dependencies[index]),
            .scope_bindings = bindings == scope_bindings_.end()
                                ? std::nullopt
                                : std::optional{std::span<StringId const>{bindings->second}}};
}

auto CppIR::namespace_node(StringId name, std::span<NodeId const> children) -> NodeId {
    auto const index{namespace_nodes_.name.size()};
    namespace_nodes_.name.push_back(name);
    namespace_nodes_.children.push_back(save_nodes(children));
    return ir_detail::node(NodeKind::namespace_, index);
}

auto CppIR::add_function(FunctionDefinition const& definition) -> FunctionId {
    assert(definition.name);
    assert(definition.kind != FunctionKind::ordinary || definition.return_type.value != 0);
    FunctionId const result{static_cast<std::uint32_t>(functions_.name.size())};
    functions_.name.push_back(definition.name);
    functions_.return_type.push_back(definition.return_type);
    functions_.return_syntax.push_back(definition.return_syntax);
    functions_.kind.push_back(definition.kind);
    auto const& implementation{definition.implementation};
    std::uint32_t body_index{};
    if (implementation.kind == FunctionImplementationKind::body) {
        body_index = static_cast<std::uint32_t>(function_bodies_.block.size());
        function_bodies_.block.push_back(block(implementation.body));
        function_bodies_.initializers.push_back(
            ir::save_range(initializer_references_, std::span{implementation.initializers}));
    }
    functions_.implementation.push_back(FunctionImplementationId{
        (static_cast<std::uint32_t>(implementation.kind) << 24) | body_index});

    auto const exception{definition.exception_specification};
    std::uint32_t exception_index{};
    if (exception.kind == FunctionExceptionKind::conditional) {
        assert(exception.condition);
        exception_index = static_cast<std::uint32_t>(conditional_noexcept_.size());
        conditional_noexcept_.push_back(exception.condition);
    }
    functions_.exception_specification.push_back(
        FunctionExceptionId{(static_cast<std::uint32_t>(exception.kind) << 24) | exception_index});

    functions_.receiver.push_back(definition.receiver);
    auto const flags{definition.flags};
    functions_.flags.emplace_back(flags.is_constexpr,
                                  flags.is_inline,
                                  flags.is_nodiscard,
                                  flags.define_in_header,
                                  flags.is_noreturn);
    functions_.export_specifier.push_back(definition.export_specifier);
    functions_.template_declaration.push_back(definition.template_declaration);
    functions_.requires_clause.push_back(definition.requires_clause);
    functions_.formatting.push_back(definition.formatting);
    functions_.parameters.push_back(
        ir::save_range(parameter_references_, std::span{definition.parameters}));
    functions_.dependencies.push_back(save_dependencies(definition.dependencies));
    return result;
}

auto CppIR::get_view(FunctionId function) const -> FunctionView {
    auto const index{function.value};
    auto const implementation_id{functions_.implementation[index]};
    FunctionImplementationView implementation{.kind = implementation_id.kind()};
    if (implementation.kind == FunctionImplementationKind::body) {
        auto const body_index{implementation_id.index()};
        implementation.body =
            nodes(block_nodes_.children[function_bodies_.block[body_index].index()]);
        implementation.initializers =
            ir::range_values(initializer_references_, function_bodies_.initializers[body_index]);
    }

    auto const exception_id{functions_.exception_specification[index]};
    FunctionExceptionSpecification exception{.kind = exception_id.kind()};
    if (exception.kind == FunctionExceptionKind::conditional) {
        exception.condition = conditional_noexcept_[exception_id.index()];
    }

    return {
        .name = functions_.name[index],
        .return_type = functions_.return_type[index],
        .return_syntax = functions_.return_syntax[index],
        .kind = functions_.kind[index],
        .parameters = ir::range_values(parameter_references_, functions_.parameters[index]),
        .implementation = implementation,
        .exception_specification = exception,
        .receiver = functions_.receiver[index],
        .flags = functions_.flags[index],
        .export_specifier = functions_.export_specifier[index],
        .template_declaration = functions_.template_declaration[index],
        .requires_clause = functions_.requires_clause[index],
        .formatting = functions_.formatting[index],
        .dependencies = dependency_list(functions_.dependencies[index]),
    };
}

auto CppIR::get_view(FunctionParameterId parameter) const -> FunctionParameterView {
    auto const index{parameter.value};
    return {.type = parameters_.type[index],
            .name = parameters_.name[index],
            .default_value = parameters_.default_value[index]};
}

auto CppIR::parameter(CppTypeId type, StringId name, Syntax default_value) -> FunctionParameterId {
    FunctionParameterId const result{static_cast<std::uint32_t>(parameters_.name.size())};
    parameters_.type.push_back(type);
    parameters_.name.push_back(name);
    parameters_.default_value.push_back(default_value);
    return result;
}

auto CppIR::parameter(CppTypeId type, std::string_view name, Syntax default_value)
    -> FunctionParameterId {
    return parameter(type, strings.intern(name), default_value);
}

auto CppIR::parameter(std::string_view type, std::string_view name, Syntax default_value)
    -> FunctionParameterId {
    return parameter(types.intern(type), strings.intern(name), default_value);
}

auto CppIR::member_initializer(StringId member, StringId value) -> MemberInitializerId {
    return member_initializer(member, authored_fragment(value));
}

auto CppIR::member_initializer(StringId member, Syntax value) -> MemberInitializerId {
    MemberInitializerId const result{static_cast<std::uint32_t>(initializers_.member.size())};
    initializers_.member.push_back(member);
    initializers_.value.push_back(value);
    return result;
}

auto CppIR::function_node(FunctionId function,
                          std::optional<CppTypeId> owner,
                          bool declaration,
                          bool header,
                          std::optional<ReturnSyntax> return_syntax) -> NodeId {
    if (owner &&
        functions_.implementation[function.value].kind() == FunctionImplementationKind::deleted) {
        throw std::invalid_argument{"A deleted function cannot be an out-of-line definition"};
    }
    assert(declaration ||
           functions_.implementation[function.value].kind() != FunctionImplementationKind::none);
    auto const index{function_nodes_.function.size()};
    function_nodes_.function.push_back(function);
    function_nodes_.owner.push_back(owner);
    function_nodes_.declaration.push_back(declaration);
    function_nodes_.is_header.push_back(header);
    function_nodes_.return_syntax.push_back(return_syntax);
    return ir_detail::node(NodeKind::function, index);
}

auto CppIR::lines(int count) -> NodeId {
    assert(count > 0);
    auto const index{new_lines_nodes_.count.size()};
    new_lines_nodes_.count.push_back(count);
    return ir_detail::node(NodeKind::new_lines, index);
}

auto CppIR::declaration(FunctionId function) -> NodeId {
    return function_node(function, {}, true, false, {});
}

auto CppIR::header_function(FunctionId function) -> NodeId {
    return function_node(
        function, {}, !functions_.flags[function.value].define_in_header(), true, {});
}

auto CppIR::definition(FunctionId function,
                       std::optional<CppTypeId> owner,
                       std::optional<ReturnSyntax> syntax,
                       bool header) -> NodeId {
    return function_node(function, owner, false, header, syntax);
}
void CppIR::clear() {
    for (auto& types_for_kind : expression_types_) {
        types_for_kind.clear();
    }
    symbol_references_.clear();
    pack_patterns_.clear();
    authored_fragments_.clear();
    scope_name_owners_.clear();
    scope_name_members_.clear();
    scope_bindings_.clear();
    auto clear = [](auto&... columns) { (columns.clear(), ...); };
    clear(line_directive_nodes_.line, line_directive_nodes_.path);
    clear(for_stmt_nodes_.initializer,
          for_stmt_nodes_.condition,
          for_stmt_nodes_.increment,
          for_stmt_nodes_.children);
    clear(files, modules, lowering, template_parameter_references_);
    clear(type_template_parameters_.name,
          type_template_parameters_.default_type,
          type_template_parameters_.pack);
    clear(value_template_parameters_.type,
          value_template_parameters_.name,
          value_template_parameters_.default_value,
          value_template_parameters_.pack);
    clear(templates_.parameters, templates_.authored_text);
    clear(template_id_exprs_.callee, template_id_exprs_.arguments);
    clear(parenthesized_exprs_.operand);
    clear(parameter_references_,
          initializer_references_,
          case_references_,
          enumerator_references_,
          children_,
          arguments_,
          type_references_,
          dependencies_);
    clear(raw_nodes_.text, raw_nodes_.parts, raw_nodes_.dependencies);
    clear(raw_lines_nodes_.lines, raw_lines_nodes_.dependencies);
    clear(raw_lines_);
    clear(line_comment_nodes_.text);
    clear(block_comment_nodes_.text);
    clear(block_nodes_.children);
    clear(if_stmt_nodes_.condition,
          if_stmt_nodes_.then_children,
          if_stmt_nodes_.else_children,
          if_stmt_nodes_.is_constexpr);
    clear(switch_stmt_nodes_.condition, switch_stmt_nodes_.cases);
    clear(expr_stmt_nodes_.expr, expr_stmt_nodes_.dependencies);
    clear(declaration_macro_nodes_.invocation,
          declaration_macro_nodes_.body,
          declaration_macro_nodes_.statement);
    clear(binding_names_, binding_lists_);
    clear(return_stmt_nodes_.expr, return_stmt_nodes_.dependencies);
    clear(assignment_stmt_nodes_.target,
          assignment_stmt_nodes_.value,
          assignment_stmt_nodes_.dependencies);
    clear(variable_declaration_stmt_nodes_.type,
          variable_declaration_stmt_nodes_.name,
          variable_declaration_stmt_nodes_.initializer,
          variable_declaration_stmt_nodes_.template_declaration,
          variable_declaration_stmt_nodes_.is_inline,
          variable_declaration_stmt_nodes_.is_constexpr);
    clear(static_assert_nodes_.condition, static_assert_nodes_.message);
    clear(new_lines_nodes_.count);
    clear(access_specifier_nodes_.access, access_specifier_nodes_.indentation);
    clear(include_nodes_.path, include_nodes_.style);
    clear(forward_declaration_nodes_.name,
          forward_declaration_nodes_.kind,
          forward_declaration_nodes_.template_declaration);
    clear(friend_declaration_nodes_.name,
          friend_declaration_nodes_.kind,
          friend_declaration_nodes_.type,
          friend_declaration_nodes_.template_declaration);
    clear(using_declaration_nodes_.name,
          using_declaration_nodes_.type,
          using_declaration_nodes_.template_declaration);
    clear(instance_members_.type, instance_members_.name, instance_members_.initializer);
    clear(static_members_.type,
          static_members_.name,
          static_members_.initializer,
          static_members_.template_declaration,
          static_members_.is_inline);
    clear(constexpr_members_.type,
          constexpr_members_.name,
          constexpr_members_.initializer,
          constexpr_members_.template_declaration);
    clear(function_nodes_.function,
          function_nodes_.owner,
          function_nodes_.declaration,
          function_nodes_.is_header,
          function_nodes_.return_syntax);
    clear(enum_nodes_.name,
          enum_nodes_.underlying_type,
          enum_nodes_.values,
          enum_nodes_.export_specifier,
          enum_nodes_.scoped);
    clear(struct_nodes_.name,
          struct_nodes_.children,
          struct_nodes_.bases,
          struct_nodes_.template_declaration,
          struct_nodes_.requires_clause,
          struct_nodes_.export_specifier,
          struct_nodes_.record_kind,
          struct_nodes_.dependencies);
    clear(namespace_nodes_.name, namespace_nodes_.children);
    clear(named_exprs_.spelling, named_exprs_.dependencies);
    clear(literal_exprs_.spelling);
    clear(numeric_literal_exprs_.i8,
          numeric_literal_exprs_.i16,
          numeric_literal_exprs_.i32,
          numeric_literal_exprs_.i64,
          numeric_literal_exprs_.u8,
          numeric_literal_exprs_.u16,
          numeric_literal_exprs_.u32,
          numeric_literal_exprs_.u64,
          numeric_literal_exprs_.f32,
          numeric_literal_exprs_.f64);
    clear(string_literal_exprs_);
    clear(call_exprs_.callee, call_exprs_.arguments);
    clear(member_exprs_.object, member_exprs_.member, member_exprs_.through_pointer);
    clear(subscript_exprs_.object, subscript_exprs_.index);
    clear(binary_exprs_.operation,
          binary_exprs_.left,
          binary_exprs_.right,
          binary_exprs_.line_break_after);
    clear(unary_exprs_.operation, unary_exprs_.operand);
    clear(cast_exprs_.type, cast_exprs_.operand);
    clear(
        conditional_exprs_.condition, conditional_exprs_.when_true, conditional_exprs_.when_false);
    clear(sizeof_exprs_.type);
    clear(lambda_exprs_.captures,
          lambda_exprs_.parameters,
          lambda_exprs_.body,
          lambda_exprs_.template_declaration);
    clear(initializer_exprs_.type, initializer_exprs_.elements);
    clear(designated_initializers_.member, designated_initializers_.value);
    clear(raw_exprs_.text, raw_exprs_.dependencies);
    clear(parameters_.type, parameters_.name, parameters_.default_value);
    clear(initializers_.member, initializers_.value);
    clear(enumerators_.name, enumerators_.initializer, enumerators_.annotation);
    clear(cases_.label, cases_.body);
    clear(functions_.name,
          functions_.return_type,
          functions_.parameters,
          functions_.return_syntax,
          functions_.kind,
          functions_.implementation,
          functions_.exception_specification,
          functions_.receiver);
    clear(function_bodies_.block, function_bodies_.initializers);
    clear(conditional_noexcept_);
    clear(functions_.flags,
          functions_.export_specifier,
          functions_.template_declaration,
          functions_.requires_clause,
          functions_.formatting,
          functions_.dependencies);
}

auto CppIR::raw(std::string_view value, std::span<TypeDependencyId const> dependencies) -> NodeId {
    return raw(strings.intern(value), dependencies);
}
auto CppIR::raw(StringId value, std::span<TypeDependencyId const> dependencies) -> NodeId {
    auto const index{raw_nodes_.text.size()};
    raw_nodes_.text.push_back(value);
    raw_nodes_.parts.push_back({});
    raw_nodes_.dependencies.push_back(save_dependencies(dependencies));
    return ir_detail::node(NodeKind::raw, index);
}
auto CppIR::raw_parts(std::span<Syntax const> parts, std::span<TypeDependencyId const> dependencies)
    -> NodeId {
    auto const index{raw_nodes_.text.size()};
    raw_nodes_.text.push_back({});
    raw_nodes_.parts.push_back(save_exprs(parts));
    raw_nodes_.dependencies.push_back(save_dependencies(dependencies));
    return ir_detail::node(NodeKind::raw, index);
}
auto CppIR::raw_lines(std::span<StringId const> lines,
                      std::span<TypeDependencyId const> dependencies) -> NodeId {
    auto const index{raw_lines_nodes_.lines.size()};
    raw_lines_nodes_.lines.push_back(ir::save_range(raw_lines_, lines));
    raw_lines_nodes_.dependencies.push_back(save_dependencies(dependencies));
    return ir_detail::node(NodeKind::raw_lines, index);
}

auto CppIR::lambda(std::span<Syntax const> captures,
                   std::span<FunctionParameterId const> parameters,
                   BlockId body,
                   TemplateId template_declaration) -> Expr {
    auto const result{expr_detail::handle(ExprKind::lambda, lambda_exprs_.body.size())};
    lambda_exprs_.captures.push_back(save_exprs(captures));
    lambda_exprs_.parameters.push_back(ir::save_range(parameter_references_, parameters));
    lambda_exprs_.body.push_back(block_nodes_.children[body.index()]);
    lambda_exprs_.template_declaration.push_back(template_declaration);
    std::vector<TypeDependencyId> dependencies;
    collect_dependencies(result, dependencies);
    return finish_expression(result, types.closure_type(result, dependencies));
}

auto CppIR::name(std::string_view value, std::span<TypeDependencyId const> dependencies) -> Syntax {
    return name(strings.intern(value), dependencies);
}
auto CppIR::name(StringId value, std::span<TypeDependencyId const> dependencies) -> Syntax {
    assert(!strings.view(value).contains("::"));
    auto const index{static_cast<std::uint32_t>(named_exprs_.spelling.size())};
    named_exprs_.spelling.push_back(value);
    named_exprs_.dependencies.push_back(save_dependencies(dependencies));
    return {SyntaxKind::name, index};
}
auto CppIR::named(std::string_view value, std::span<TypeDependencyId const> dependencies) -> Expr {
    return dependent_reference(name(value, dependencies));
}
auto CppIR::named(StringId value, std::span<TypeDependencyId const> dependencies) -> Expr {
    return dependent_reference(name(value, dependencies));
}
auto CppIR::nullptr_literal() -> Expr {
    return literal(types.named("std::nullptr_t", {types.dependency("std::nullptr_t", "cstddef")}),
                   "nullptr");
}
auto CppIR::boolean_literal(bool const value) -> Expr {
    return finish_expression(expr_detail::handle(ExprKind::boolean_literal, value),
                             types.std.bool_);
}
template <ir::NumericLiteralValue Value>
auto CppIR::numeric_literal(Value const number) -> Expr {
    if constexpr (std::floating_point<Value>) {
        if (!std::isfinite(number)) {
            auto const scalar{std::same_as<Value, float> ? types.std.f32 : types.std.f64};
            auto const limits{types.template_type(types.std.numeric_limits, {scalar})};
            auto const result{
                call(scope_name(limits, std::isnan(number) ? "quiet_NaN" : "infinity"))};
            return std::signbit(number) ? unary(UnaryOperator::negate, result) : result;
        }
        if (std::signbit(number)) {
            return unary(UnaryOperator::negate, literal(-number));
        }
    } else if constexpr (std::signed_integral<Value> && sizeof(Value) >= sizeof(std::int32_t)) {
        if (number == std::numeric_limits<Value>::min()) {
            return parenthesized(
                binary(BinaryOperator::subtract,
                       unary(UnaryOperator::negate, literal(std::numeric_limits<Value>::max())),
                       literal(1)));
        }
        if (number < 0) {
            return unary(UnaryOperator::negate, literal(-number));
        }
    }

    auto const store = [this, number](auto& values, ExprKind const kind, CppTypeId const type) {
        auto const result{expr_detail::handle(kind, values.size())};
        values.push_back(number);
        return finish_expression(result, type);
    };
    if constexpr (std::same_as<Value, std::int8_t>) {
        return store(numeric_literal_exprs_.i8, ExprKind::i8_literal, types.std.i8);
    } else if constexpr (std::same_as<Value, std::int16_t>) {
        return store(numeric_literal_exprs_.i16, ExprKind::i16_literal, types.std.i16);
    } else if constexpr (std::same_as<Value, std::int32_t>) {
        return store(numeric_literal_exprs_.i32, ExprKind::i32_literal, types.std.i32);
    } else if constexpr (std::same_as<Value, std::int64_t>) {
        return store(numeric_literal_exprs_.i64, ExprKind::i64_literal, types.std.i64);
    } else if constexpr (std::same_as<Value, std::uint8_t>) {
        return store(numeric_literal_exprs_.u8, ExprKind::u8_literal, types.std.u8);
    } else if constexpr (std::same_as<Value, std::uint16_t>) {
        return store(numeric_literal_exprs_.u16, ExprKind::u16_literal, types.std.u16);
    } else if constexpr (std::same_as<Value, std::uint32_t>) {
        return store(numeric_literal_exprs_.u32, ExprKind::u32_literal, types.std.u32);
    } else if constexpr (std::same_as<Value, std::uint64_t>) {
        return store(numeric_literal_exprs_.u64, ExprKind::u64_literal, types.std.u64);
    } else if constexpr (std::same_as<Value, float>) {
        return store(numeric_literal_exprs_.f32, ExprKind::f32_literal, types.std.f32);
    } else if constexpr (std::same_as<Value, double>) {
        return store(numeric_literal_exprs_.f64, ExprKind::f64_literal, types.std.f64);
    }
}

template auto CppIR::numeric_literal(std::int8_t) -> Expr;
template auto CppIR::numeric_literal(std::int16_t) -> Expr;
template auto CppIR::numeric_literal(std::int32_t) -> Expr;
template auto CppIR::numeric_literal(std::int64_t) -> Expr;
template auto CppIR::numeric_literal(std::uint8_t) -> Expr;
template auto CppIR::numeric_literal(std::uint16_t) -> Expr;
template auto CppIR::numeric_literal(std::uint32_t) -> Expr;
template auto CppIR::numeric_literal(std::uint64_t) -> Expr;
template auto CppIR::numeric_literal(float) -> Expr;
template auto CppIR::numeric_literal(double) -> Expr;

auto CppIR::literal(CppTypeId type, std::string_view spelling) -> Expr {
    if (spelling.starts_with('-')) {
        return unary(UnaryOperator::negate, literal(type, spelling.substr(1)));
    }
    auto const value{strings.intern(spelling)};
    auto const result{expr_detail::handle(ExprKind::literal, literal_exprs_.spelling.size())};
    literal_exprs_.spelling.push_back(value);
    return finish_expression(result, type);
}
auto CppIR::string_literal(std::string_view value) -> Expr {
    return string_literal(strings.intern(value));
}
auto CppIR::string_literal(StringId value) -> Expr {
    auto const result{expr_detail::handle(ExprKind::string_literal, string_literal_exprs_.size())};
    string_literal_exprs_.push_back(value);
    return finish_expression(
        result,
        types.decorate(
            types.std.char_, "", " const[" + std::to_string(strings.view(value).size() + 1) + "]"));
}
auto CppIR::raw_expr(CppTypeId type,
                     std::string_view value,
                     std::span<TypeDependencyId const> dependencies) -> Expr {
    return raw_expr(type, strings.intern(value), dependencies);
}
auto CppIR::raw_expr(CppTypeId type, StringId value, std::span<TypeDependencyId const> dependencies)
    -> Expr {
    auto const result{expr_detail::handle(ExprKind::raw, raw_exprs_.text.size())};
    raw_exprs_.text.push_back(value);
    raw_exprs_.dependencies.push_back(save_dependencies(dependencies));
    return finish_expression(result, type);
}
auto CppIR::call(Syntax callee, std::span<Syntax const> arguments) -> Expr {
    auto const result{expr_detail::handle(ExprKind::call, call_exprs_.callee.size())};
    call_exprs_.callee.push_back(callee);
    call_exprs_.arguments.push_back(save_exprs(arguments));
    return dependent_expression(result);
}
auto CppIR::member_access(Expr object, std::string_view member) -> Expr {
    return member_access(object, strings.intern(member));
}
auto CppIR::member_access(Expr object, StringId member) -> Expr {
    auto const result{expr_detail::handle(ExprKind::member_access, member_exprs_.object.size())};
    member_exprs_.object.push_back(object);
    member_exprs_.member.push_back(member);
    member_exprs_.through_pointer.push_back(false);
    return dependent_expression(result);
}
auto CppIR::scope_member(CppTypeId const owner, std::string_view const member) -> Expr {
    return scope_member(owner, strings.intern(member));
}
auto CppIR::scope_name(CppTypeId owner, std::string_view member) -> Syntax {
    return scope_name(owner, strings.intern(member));
}
auto CppIR::scope_name(CppTypeId owner, StringId member) -> Syntax {
    assert(owner.value != 0);
    auto const index{static_cast<std::uint32_t>(scope_name_owners_.size())};
    scope_name_owners_.push_back(owner);
    scope_name_members_.push_back(member);
    return {SyntaxKind::scope_name, index};
}
auto CppIR::scope_member(CppTypeId const owner, StringId const member) -> Expr {
    return dependent_reference(scope_name(owner, member));
}
auto CppIR::pointer_member_access(Expr object, std::string_view member) -> Expr {
    return pointer_member_access(object, strings.intern(member));
}
auto CppIR::pointer_member_access(Expr object, StringId member) -> Expr {
    auto const result{member_access(object, member)};
    member_exprs_.through_pointer[result.index()] = true;
    return result;
}
auto CppIR::unary(UnaryOperator operation, Expr operand) -> Expr {
    auto const result{expr_detail::handle(ExprKind::unary, unary_exprs_.operation.size())};
    unary_exprs_.operation.push_back(operation);
    unary_exprs_.operand.push_back(operand);
    return dependent_expression(result);
}
auto CppIR::subscript(Expr object, Expr index) -> Expr {
    auto const result{expr_detail::handle(ExprKind::subscript, subscript_exprs_.object.size())};
    subscript_exprs_.object.push_back(object);
    subscript_exprs_.index.push_back(index);
    return dependent_expression(result);
}
auto CppIR::address_of(Syntax symbol) -> Expr {
    auto const result{expr_detail::handle(ExprKind::unary, unary_exprs_.operation.size())};
    unary_exprs_.operation.push_back(UnaryOperator::address_of);
    unary_exprs_.operand.push_back(symbol);
    return dependent_expression(result);
}
auto CppIR::binary(BinaryOperator operation, Expr left, Expr right, bool line_break_after) -> Expr {
    auto const result{expr_detail::handle(ExprKind::binary, binary_exprs_.operation.size())};
    binary_exprs_.operation.push_back(operation);
    binary_exprs_.left.push_back(left);
    binary_exprs_.right.push_back(right);
    binary_exprs_.line_break_after.push_back(line_break_after);
    return dependent_expression(result);
}
auto CppIR::conditional(Expr condition, Expr when_true, Expr when_false) -> Expr {
    auto const result{
        expr_detail::handle(ExprKind::conditional, conditional_exprs_.condition.size())};
    conditional_exprs_.condition.push_back(condition);
    conditional_exprs_.when_true.push_back(when_true);
    conditional_exprs_.when_false.push_back(when_false);
    return dependent_expression(result);
}
auto CppIR::static_cast_expr(CppTypeId type, Expr operand) -> Expr {
    auto const result{expr_detail::handle(ExprKind::static_cast_, cast_exprs_.type.size())};
    cast_exprs_.type.push_back(type);
    cast_exprs_.operand.push_back(operand);
    return finish_expression(result, type);
}
auto CppIR::sizeof_type(CppTypeId type) -> Expr {
    auto const result{expr_detail::handle(ExprKind::sizeof_type, sizeof_exprs_.type.size())};
    sizeof_exprs_.type.push_back(type);
    return finish_expression(result, types.std.size_t);
}
auto CppIR::reinterpret_cast_expr(CppTypeId type, Expr operand) -> Expr {
    auto const result{expr_detail::handle(ExprKind::reinterpret_cast_, cast_exprs_.type.size())};
    cast_exprs_.type.push_back(type);
    cast_exprs_.operand.push_back(operand);
    return finish_expression(result, type);
}
auto CppIR::init_list(std::span<Syntax const> elements) -> Syntax {
    auto const index{static_cast<std::uint32_t>(initializer_exprs_.type.size())};
    initializer_exprs_.type.push_back(std::nullopt);
    initializer_exprs_.elements.push_back(save_exprs(elements));
    return {SyntaxKind::initializer_list, index};
}
auto CppIR::init_list(std::span<Syntax const> elements, CppTypeId type) -> Expr {
    assert(type.value != 0);
    auto const result{
        expr_detail::handle(ExprKind::initializer_list, initializer_exprs_.type.size())};
    initializer_exprs_.type.push_back(type);
    initializer_exprs_.elements.push_back(save_exprs(elements));
    // Preserve deduction when the written type is a template name such as std::array.
    return dependent_expression(result);
}
auto CppIR::designated_initializer(StringId member, Syntax value) -> Syntax {
    assert(member.value != 0 && value);
    auto const result{Syntax{SyntaxKind::designated_initializer,
                             static_cast<std::uint32_t>(designated_initializers_.member.size())}};
    designated_initializers_.member.push_back(member);
    designated_initializers_.value.push_back(value);
    return result;
}
auto CppIR::type_reference(CppTypeId type) -> Syntax {
    assert(type.value != 0);
    return {SyntaxKind::type_reference, type.value};
}
auto CppIR::decltype_type(Expr const operand) -> CppTypeId {
    std::vector<TypeDependencyId> dependencies;
    collect_dependencies(operand, dependencies);
    return types.decltype_type(operand, dependencies);
}
auto CppIR::template_type(CppTypeId const primary, std::span<Syntax const> const arguments)
    -> CppTypeId {
    auto const expression{template_id(type_reference(primary), arguments)};
    std::vector<TypeDependencyId> dependencies;
    collect_dependencies(expression, dependencies);
    return types.template_type(expression, dependencies);
}
auto CppIR::parenthesized(Expr expr) -> Expr {
    auto const result{
        expr_detail::handle(ExprKind::parenthesized, parenthesized_exprs_.operand.size())};
    parenthesized_exprs_.operand.push_back(expr);
    auto const type{expression_type(expr)};
    if (types.shapes_[type.value - 1].form == CppTypeForm::closure) {
        return finish_expression(result, type);
    }
    return dependent_expression(result);
}
auto CppIR::template_id(Syntax callee, std::span<Syntax const> arguments) -> Syntax {
    auto const result{Syntax{SyntaxKind::template_id,
                             static_cast<std::uint32_t>(template_id_exprs_.callee.size())}};
    template_id_exprs_.callee.push_back(callee);
    template_id_exprs_.arguments.push_back(save_exprs(arguments));
    return result;
}

auto CppIR::type_template_parameter(StringId name, CppTypeId default_type, bool pack)
    -> TemplateParameterId {
    auto const index{static_cast<std::uint32_t>(type_template_parameters_.name.size())};
    type_template_parameters_.name.push_back(name);
    type_template_parameters_.default_type.push_back(default_type);
    type_template_parameters_.pack.push_back(pack);
    return TemplateParameterId{(static_cast<std::uint32_t>(TemplateParameterKind::type) << 24) |
                               index};
}
auto CppIR::value_template_parameter(CppTypeId type, StringId name, Syntax default_value, bool pack)
    -> TemplateParameterId {
    auto const index{static_cast<std::uint32_t>(value_template_parameters_.name.size())};
    value_template_parameters_.type.push_back(type);
    value_template_parameters_.name.push_back(name);
    value_template_parameters_.default_value.push_back(default_value);
    value_template_parameters_.pack.push_back(pack);
    return TemplateParameterId{(static_cast<std::uint32_t>(TemplateParameterKind::value) << 24) |
                               index};
}
auto CppIR::template_declaration(std::span<TemplateParameterId const> parameters) -> TemplateId {
    assert(!parameters.empty());
    auto const index{static_cast<std::uint32_t>(templates_.parameters.size())};
    templates_.parameters.push_back(ir::save_range(template_parameter_references_, parameters));
    return TemplateId{(static_cast<std::uint32_t>(TemplateKind::parameters) << 24) | index};
}
auto CppIR::template_specialization() -> TemplateId {
    return TemplateId{static_cast<std::uint32_t>(TemplateKind::explicit_specialization) << 24};
}
auto CppIR::raw_template_declaration(StringId parameters) -> TemplateId {
    auto const index{static_cast<std::uint32_t>(templates_.authored_text.size())};
    templates_.authored_text.push_back(parameters);
    return TemplateId{(static_cast<std::uint32_t>(TemplateKind::authored_raw) << 24) | index};
}

void CppIR::set_scope_bindings(NodeId node, std::span<StringId const> names) {
    scope_bindings_[node.value] = {names.begin(), names.end()};
}

/* **************************************** */
// Normalization
/* **************************************** */
void CppIR::merge_adjacent_namespaces(CppFileId const file) {
    auto const roots{files.nodes(file)};
    std::vector<NodeId> values{roots.begin(), roots.end()};
    if (merge_adjacent_namespaces(values)) {
        files.set_nodes(file, values);
    }
}
auto CppIR::merge_adjacent_namespaces(std::vector<NodeId>& values) -> bool {
    bool changed{};
    std::size_t output{};
    auto const count{values.size()};
    for (std::size_t first{}; first < count;) {
        auto const node{values[first]};
        if (node.kind() != NodeKind::namespace_) {
            values[output++] = node;
            ++first;
            continue;
        }

        auto const name{namespace_nodes_.name[node.index()]};
        auto const initial_children{nodes(namespace_nodes_.children[node.index()])};
        std::vector<NodeId> children{initial_children.begin(), initial_children.end()};
        auto end{first + 1};
        while (end < count) {
            auto next{end};
            while (next < count && values[next].kind() == NodeKind::new_lines) {
                ++next;
            }
            if (next == count || values[next].kind() != NodeKind::namespace_ ||
                namespace_nodes_.name[values[next].index()] != name) {
                break;
            }

            for (auto spacing{end}; spacing < next; ++spacing) {
                children.push_back(values[spacing]);
            }
            auto const next_children{nodes(namespace_nodes_.children[values[next].index()])};
            children.insert(children.end(), next_children.begin(), next_children.end());
            end = next + 1;
        }

        auto const children_changed{merge_adjacent_namespaces(children)};
        auto replacement{node};
        if (end != first + 1 || children_changed) {
            replacement = namespace_node(name, children);
            std::vector<StringId> bindings;
            for (auto index{first}; index < end; ++index) {
                if (auto const found{scope_bindings_.find(values[index].value)};
                    found != scope_bindings_.end()) {
                    bindings.insert(bindings.end(), found->second.begin(), found->second.end());
                }
            }
            if (!bindings.empty()) {
                set_scope_bindings(replacement, bindings);
            }
            changed = true;
        }
        values[output++] = replacement;
        first = end;
    }
    values.resize(output);
    return changed;
}

/* **************************************** */
// Traversal and dependencies
/* **************************************** */
void CppIR::for_each_child(NodeId node, std::function<void(NodeId)> const& visit) const {
    auto visit_range = [&](ir::Range range) {
        for (auto const child : nodes(range)) {
            visit(child);
        }
    };
    auto const i{node.index()};
    switch (node.kind()) {
        case NodeKind::declaration_macro:
            if (auto const body{declaration_macro_nodes_.body[i]}) {
                visit_range(block_nodes_.children[body->index()]);
            }
            break;
        case NodeKind::block:
            visit_range(block_nodes_.children[i]);
            break;
        case NodeKind::struct_:
            visit_range(struct_nodes_.children[i]);
            break;
        case NodeKind::namespace_:
            visit_range(namespace_nodes_.children[i]);
            break;
        case NodeKind::function: {
            auto const implementation{functions_.implementation[function_nodes_.function[i].value]};
            if (!function_nodes_.declaration[i] &&
                implementation.kind() == FunctionImplementationKind::body) {
                visit_range(
                    block_nodes_.children[function_bodies_.block[implementation.index()].index()]);
            }
            break;
        }
        case NodeKind::if_stmt:
            visit_range(if_stmt_nodes_.then_children[i]);
            if (auto const otherwise{if_stmt_nodes_.else_children[i]}) {
                visit_range(*otherwise);
            }
            break;
        case NodeKind::for_stmt:
            visit(for_stmt_nodes_.initializer[i]);
            visit_range(for_stmt_nodes_.children[i]);
            break;
        case NodeKind::switch_stmt: {
            auto const range{switch_stmt_nodes_.cases[i]};
            auto const end{range.begin + range.count};
            for (auto c{range.begin}; c < end; ++c) {
                visit_range(cases_.body[case_references_[c].value]);
            }
            break;
        }
        default:
            break;
    }
}
void CppIR::collect_dependencies(NodeId node, std::vector<TypeDependencyId>& result) const {
    auto append = [&](std::span<TypeDependencyId const> values) {
        result.insert(result.end(), values.begin(), values.end());
    };
    auto type = [&](CppTypeId id) { append(types.dependencies(id)); };
    auto const i{node.index()};
    switch (node.kind()) {
        case NodeKind::raw:
            append(dependency_list(raw_nodes_.dependencies[i]));
            for (auto const part : exprs(raw_nodes_.parts[i])) {
                collect_dependencies(part, result);
            }
            break;
        case NodeKind::raw_lines:
            append(dependency_list(raw_lines_nodes_.dependencies[i]));
            break;
        case NodeKind::static_assert_:
            collect_dependencies(static_assert_nodes_.condition[i], result);
            break;
        case NodeKind::expr_stmt:
            append(dependency_list(expr_stmt_nodes_.dependencies[i]));
            collect_dependencies(expr_stmt_nodes_.expr[i], result);
            break;
        case NodeKind::declaration_macro:
            collect_dependencies(declaration_macro_nodes_.invocation[i], result);
            break;
        case NodeKind::return_stmt:
            append(dependency_list(return_stmt_nodes_.dependencies[i]));
            if (auto const value{return_stmt_nodes_.expr[i]}) {
                collect_dependencies(*value, result);
            }
            break;
        case NodeKind::assignment_stmt:
            append(dependency_list(assignment_stmt_nodes_.dependencies[i]));
            collect_dependencies(assignment_stmt_nodes_.target[i], result);
            collect_dependencies(assignment_stmt_nodes_.value[i], result);
            break;
        case NodeKind::variable_declaration_stmt:
            collect_dependencies(variable_declaration_stmt_nodes_.template_declaration[i], result);
            type(variable_declaration_stmt_nodes_.type[i]);
            collect_dependencies(variable_declaration_stmt_nodes_.name[i], result);
            if (auto const initializer{variable_declaration_stmt_nodes_.initializer[i]}) {
                collect_dependencies(*initializer, result);
            }
            break;
        case NodeKind::if_stmt:
            collect_dependencies(if_stmt_nodes_.condition[i], result);
            break;
        case NodeKind::for_stmt:
            collect_dependencies(for_stmt_nodes_.condition[i], result);
            collect_dependencies(for_stmt_nodes_.increment[i], result);
            break;
        case NodeKind::switch_stmt: {
            collect_dependencies(switch_stmt_nodes_.condition[i], result);
            auto const range{switch_stmt_nodes_.cases[i]};
            for (auto const case_id : ir::range_values(case_references_, range)) {
                if (auto const label{cases_.label[case_id.value]}) {
                    collect_dependencies(*label, result);
                }
            }
            break;
        }
        case NodeKind::forward_declaration:
            collect_dependencies(forward_declaration_nodes_.template_declaration[i], result);
            break;
        case NodeKind::friend_declaration:
            collect_dependencies(friend_declaration_nodes_.template_declaration[i], result);
            if (auto const friend_type{friend_declaration_nodes_.type[i]}; friend_type.value != 0) {
                type(friend_type);
            }
            break;
        case NodeKind::using_declaration:
            collect_dependencies(using_declaration_nodes_.template_declaration[i], result);
            type(using_declaration_nodes_.type[i]);
            break;
        case NodeKind::instance_member:
            type(instance_members_.type[i]);
            if (auto const value{instance_members_.initializer[i]}) {
                collect_dependencies(value, result);
            }
            break;
        case NodeKind::static_member:
            collect_dependencies(static_members_.template_declaration[i], result);
            type(static_members_.type[i]);
            if (auto const value{static_members_.initializer[i]}) {
                collect_dependencies(value, result);
            }
            break;
        case NodeKind::constexpr_member:
            collect_dependencies(constexpr_members_.template_declaration[i], result);
            type(constexpr_members_.type[i]);
            collect_dependencies(constexpr_members_.initializer[i], result);
            break;
        case NodeKind::function: {
            auto const id{function_nodes_.function[i].value};
            if (auto const owner{function_nodes_.owner[i]}) {
                type(*owner);
            }
            collect_dependencies(functions_.name[id], result);
            collect_dependencies(functions_.template_declaration[id], result);
            if (auto const clause{functions_.requires_clause[id]}) {
                collect_dependencies(clause, result);
            }
            if (functions_.return_type[id].value != 0) {
                type(functions_.return_type[id]);
            }
            append(dependency_list(functions_.dependencies[id]));
            if (auto const exception{functions_.exception_specification[id]};
                exception.kind() == FunctionExceptionKind::conditional) {
                collect_dependencies(conditional_noexcept_[exception.index()], result);
            }
            auto const range{functions_.parameters[id]};
            for (auto const parameter : ir::range_values(parameter_references_, range)) {
                type(parameters_.type[parameter.value]);
                if (auto const value{parameters_.default_value[parameter.value]};
                    value && (function_nodes_.declaration[i] || function_nodes_.is_header[i])) {
                    collect_dependencies(value, result);
                }
            }
            auto const implementation{functions_.implementation[id]};
            if (!function_nodes_.declaration[i] &&
                implementation.kind() == FunctionImplementationKind::body) {
                auto const initializers{function_bodies_.initializers[implementation.index()]};
                for (auto const initializer :
                     ir::range_values(initializer_references_, initializers)) {
                    collect_dependencies(initializers_.value[initializer.value], result);
                }
            }
            break;
        }
        case NodeKind::enum_:
            if (enum_nodes_.underlying_type[i].value != 0) {
                type(enum_nodes_.underlying_type[i]);
            }
            for (auto const value :
                 ir::range_values(enumerator_references_, enum_nodes_.values[i])) {
                if (auto const initializer{enumerators_.initializer[value.value]}) {
                    collect_dependencies(*initializer, result);
                }
            }
            break;
        case NodeKind::struct_:
            collect_dependencies(struct_nodes_.name[i], result);
            collect_dependencies(struct_nodes_.template_declaration[i], result);
            if (auto const clause{struct_nodes_.requires_clause[i]}) {
                collect_dependencies(clause, result);
            }
            append(dependency_list(struct_nodes_.dependencies[i]));
            for (auto const base : type_list(struct_nodes_.bases[i])) {
                type(base);
            }
            break;
        default:
            break;
    }
    for_each_child(node, [&](NodeId child) { collect_dependencies(child, result); });
}

void CppIR::collect_dependencies(Expr expr, std::vector<TypeDependencyId>& result) const {
    auto const index{expr.index()};
    auto append = [&](std::span<TypeDependencyId const> values) {
        result.insert(result.end(), values.begin(), values.end());
    };
    switch (expr.kind()) {
        case ExprKind::parenthesized:
            collect_dependencies(parenthesized_exprs_.operand[index], result);
            break;
        case ExprKind::raw:
            append(dependency_list(raw_exprs_.dependencies[index]));
            break;
        case ExprKind::call:
            collect_dependencies(call_exprs_.callee[index], result);
            for (auto const child : exprs(call_exprs_.arguments[index])) {
                collect_dependencies(child, result);
            }
            break;
        case ExprKind::member_access:
            collect_dependencies(member_exprs_.object[index], result);
            break;
        case ExprKind::subscript:
            collect_dependencies(subscript_exprs_.object[index], result);
            collect_dependencies(subscript_exprs_.index[index], result);
            break;
        case ExprKind::binary:
            collect_dependencies(binary_exprs_.left[index], result);
            collect_dependencies(binary_exprs_.right[index], result);
            break;
        case ExprKind::unary:
            collect_dependencies(unary_exprs_.operand[index], result);
            break;
        case ExprKind::static_cast_:
        case ExprKind::reinterpret_cast_:
            append(types.dependencies(cast_exprs_.type[index]));
            collect_dependencies(cast_exprs_.operand[index], result);
            break;
        case ExprKind::conditional:
            collect_dependencies(conditional_exprs_.condition[index], result);
            collect_dependencies(conditional_exprs_.when_true[index], result);
            collect_dependencies(conditional_exprs_.when_false[index], result);
            break;
        case ExprKind::lambda:
            collect_dependencies(lambda_exprs_.template_declaration[index], result);
            for (auto const capture : exprs(lambda_exprs_.captures[index])) {
                collect_dependencies(capture, result);
            }
            for (auto const parameter :
                 ir::range_values(parameter_references_, lambda_exprs_.parameters[index])) {
                append(types.dependencies(parameters_.type[parameter.value]));
                if (auto const value{parameters_.default_value[parameter.value]}) {
                    collect_dependencies(value, result);
                }
            }
            for (auto const child : nodes(lambda_exprs_.body[index])) {
                collect_dependencies(child, result);
            }
            break;
        case ExprKind::sizeof_type:
            append(types.dependencies(sizeof_exprs_.type[index]));
            break;
        case ExprKind::initializer_list:
            if (auto const type{initializer_exprs_.type[index]}) {
                append(types.dependencies(*type));
            }
            for (auto const child : exprs(initializer_exprs_.elements[index])) {
                collect_dependencies(child, result);
            }
            break;
        case ExprKind::i8_literal:
        case ExprKind::i16_literal:
        case ExprKind::u8_literal:
        case ExprKind::u16_literal:
            append(types.dependencies(expression_type(expr)));
            break;
        case ExprKind::literal:
        case ExprKind::boolean_literal:
        case ExprKind::i32_literal:
        case ExprKind::i64_literal:
        case ExprKind::u32_literal:
        case ExprKind::u64_literal:
        case ExprKind::f32_literal:
        case ExprKind::f64_literal:
        case ExprKind::string_literal:
            break;
        case ExprKind::symbol_reference:
            collect_dependencies(symbol_references_[index], result);
            break;
        case ExprKind::Count:
        case ExprKind::invalid:
            std::unreachable();
    }
}

void CppIR::collect_dependencies(TemplateId declaration,
                                 std::vector<TypeDependencyId>& result) const {
    if (!declaration) {
        return;
    }
    if (declaration.kind() != TemplateKind::parameters) {
        return;
    }
    auto append_type = [&](CppTypeId type) {
        if (type.value == 0) {
            return;
        }
        auto const dependencies{types.dependencies(type)};
        result.insert(result.end(), dependencies.begin(), dependencies.end());
    };
    auto const range{templates_.parameters[declaration.index()]};
    for (auto const parameter : ir::range_values(template_parameter_references_, range)) {
        auto const index{parameter.index()};
        switch (parameter.kind()) {
            case TemplateParameterKind::type:
                append_type(type_template_parameters_.default_type[index]);
                break;
            case TemplateParameterKind::value:
                append_type(value_template_parameters_.type[index]);
                if (auto const value{value_template_parameters_.default_value[index]}) {
                    collect_dependencies(value, result);
                }
                break;
        }
    }
}

auto CppIR::expression_type(Expr expression) const -> CppTypeId {
    assert(expression);
    auto const& types_for_kind{expression_types_[expression.kind()]};
    assert(expression.index() < types_for_kind.size());
    auto const type{types_for_kind[expression.index()]};
    assert(type.value != 0);
    return type;
}
auto CppIR::finish_expression(Expr expression, CppTypeId type) -> Expr {
    assert(type.value != 0);
    auto& types_for_kind{expression_types_[expression.kind()]};
    if (types_for_kind.size() <= expression.index()) {
        types_for_kind.resize(expression.index() + 1);
    }
    types_for_kind[expression.index()] = type;
    return expression;
}
auto CppIR::dependent_expression(Expr expression) -> Expr {
    std::vector<TypeDependencyId> dependencies;
    collect_dependencies(expression, dependencies);
    return finish_expression(expression, types.decltype_type(expression, dependencies));
}
auto CppIR::reference(Syntax symbol, CppTypeId type) -> Expr {
    assert(symbol.kind() == SyntaxKind::name || symbol.kind() == SyntaxKind::scope_name ||
           symbol.kind() == SyntaxKind::member_name ||
           symbol.kind() == SyntaxKind::type_reference || symbol.kind() == SyntaxKind::template_id);
    auto const result{expr_detail::handle(ExprKind::symbol_reference, symbol_references_.size())};
    symbol_references_.push_back(symbol);
    return finish_expression(result, type);
}
auto CppIR::dependent_reference(Syntax symbol) -> Expr {
    std::vector<TypeDependencyId> dependencies;
    collect_dependencies(symbol, dependencies);
    return reference(symbol, types.decltype_type(symbol, dependencies));
}
auto CppIR::authored_fragment(std::string_view value) -> Syntax {
    return authored_fragment(strings.intern(value));
}
auto CppIR::authored_fragment(StringId value) -> Syntax {
    auto const index{static_cast<std::uint32_t>(authored_fragments_.size())};
    authored_fragments_.push_back(value);
    return {SyntaxKind::authored_fragment, index};
}
auto CppIR::member_name(Expr object, std::string_view member, bool through_pointer) -> Syntax {
    return member_name(object, strings.intern(member), through_pointer);
}
auto CppIR::member_name(Expr object, StringId member, bool through_pointer) -> Syntax {
    auto const index{static_cast<std::uint32_t>(member_exprs_.object.size())};
    member_exprs_.object.push_back(object);
    member_exprs_.member.push_back(member);
    member_exprs_.through_pointer.push_back(through_pointer);
    return {SyntaxKind::member_name, index};
}

void CppIR::collect_dependencies(Syntax syntax, std::vector<TypeDependencyId>& result) const {
    auto const index{syntax.index()};
    auto append = [&](std::span<TypeDependencyId const> dependencies) {
        result.insert(result.end(), dependencies.begin(), dependencies.end());
    };
    switch (syntax.kind()) {
        case SyntaxKind::expression:
            collect_dependencies(syntax.expression(), result);
            break;
        case SyntaxKind::name:
            append(dependency_list(named_exprs_.dependencies[index]));
            break;
        case SyntaxKind::type_reference:
            append(types.dependencies(CppTypeId{index}));
            break;
        case SyntaxKind::macro_invocation:
            collect_dependencies(call_exprs_.callee[index], result);
            for (auto argument : exprs(call_exprs_.arguments[index])) {
                collect_dependencies(argument, result);
            }
            break;
        case SyntaxKind::pack_expansion:
            collect_dependencies(pack_patterns_[index], result);
            break;
        case SyntaxKind::capture:
        case SyntaxKind::reference_capture:
        case SyntaxKind::binding_list:
            break;
        case SyntaxKind::template_id:
            collect_dependencies(template_id_exprs_.callee[index], result);
            for (auto argument : exprs(template_id_exprs_.arguments[index])) {
                collect_dependencies(argument, result);
            }
            break;
        case SyntaxKind::member_name:
            collect_dependencies(member_exprs_.object[index], result);
            break;
        case SyntaxKind::scope_name:
            append(types.dependencies(scope_name_owners_[index]));
            break;
        case SyntaxKind::initializer_list:
            for (auto element : exprs(initializer_exprs_.elements[index])) {
                collect_dependencies(element, result);
            }
            break;
        case SyntaxKind::designated_initializer:
            collect_dependencies(designated_initializers_.value[index], result);
            break;
        case SyntaxKind::authored_fragment:
            break;
        case SyntaxKind::invalid:
            std::unreachable();
    }
}

auto CppIR::macro_invocation(Syntax name, std::span<Syntax const> arguments) -> Syntax {
    auto const index{static_cast<std::uint32_t>(call_exprs_.callee.size())};
    call_exprs_.callee.push_back(name);
    call_exprs_.arguments.push_back(save_exprs(arguments));
    return {SyntaxKind::macro_invocation, index};
}
auto CppIR::macro_statement(Syntax name, std::span<Syntax const> arguments) -> NodeId {
    auto const result{declaration_macro(macro_invocation(name, arguments))};
    declaration_macro_nodes_.statement[result.index()] = true;
    return result;
}
auto CppIR::macro_block(Syntax invocation, BlockId body) -> NodeId {
    auto const result{declaration_macro(invocation)};
    declaration_macro_nodes_.body[result.index()] = body;
    return result;
}
auto CppIR::pack_expansion(Syntax pattern) -> Syntax {
    auto const index{static_cast<std::uint32_t>(pack_patterns_.size())};
    pack_patterns_.push_back(pattern);
    return {SyntaxKind::pack_expansion, index};
}
auto CppIR::capture(std::string_view name, bool by_reference) -> Syntax {
    return {by_reference ? SyntaxKind::reference_capture : SyntaxKind::capture,
            strings.intern(name).value};
}
auto CppIR::binding_list(std::initializer_list<std::string_view> names) -> Syntax {
    auto const first{static_cast<std::uint32_t>(binding_names_.size())};
    for (auto name : names) {
        binding_names_.push_back(strings.intern(name));
    }
    auto const index{static_cast<std::uint32_t>(binding_lists_.size())};
    binding_lists_.push_back({first, static_cast<std::uint32_t>(names.size())});
    return {SyntaxKind::binding_list, index};
}

} // namespace lispb::codegen
