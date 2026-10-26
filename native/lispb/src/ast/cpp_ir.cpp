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
#include <cstddef>
#include <cstdint>
#include <functional>
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
auto CppIR::save_exprs(std::span<Expr const> values) -> ir::Range {
    return ir::save_range(arguments_, values);
}
auto CppIR::exprs(ir::Range range) const -> std::span<Expr const> {
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

auto CppIR::if_stmt(Expr condition, BlockId then_block, std::optional<BlockId> else_block)
    -> NodeId {
    auto const index{if_stmt_nodes_.condition.size()};
    if_stmt_nodes_.condition.push_back(condition);
    if_stmt_nodes_.then_children.push_back(block_nodes_.children[then_block.index()]);
    if_stmt_nodes_.else_children.push_back(
        else_block ? std::optional{block_nodes_.children[else_block->index()]} : std::nullopt);
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

auto CppIR::return_stmt(std::optional<Expr> expr, std::span<TypeDependencyId const> dependencies)
    -> NodeId {
    auto const index{return_stmt_nodes_.expr.size()};
    return_stmt_nodes_.expr.push_back(expr);
    return_stmt_nodes_.dependencies.push_back(save_dependencies(dependencies));
    return ir_detail::node(NodeKind::return_stmt, index);
}

auto CppIR::assignment(Expr target, Expr value, std::span<TypeDependencyId const> dependencies)
    -> NodeId {
    auto const index{assignment_stmt_nodes_.target.size()};
    assignment_stmt_nodes_.target.push_back(target);
    assignment_stmt_nodes_.value.push_back(value);
    assignment_stmt_nodes_.dependencies.push_back(save_dependencies(dependencies));
    return ir_detail::node(NodeKind::assignment_stmt, index);
}

auto CppIR::variable(CppTypeId type, StringId name, std::optional<Expr> initializer) -> NodeId {
    auto const index{variable_declaration_stmt_nodes_.type.size()};
    variable_declaration_stmt_nodes_.type.push_back(type);
    variable_declaration_stmt_nodes_.name.push_back(name);
    variable_declaration_stmt_nodes_.initializer.push_back(initializer);
    return ir_detail::node(NodeKind::variable_declaration_stmt, index);
}

auto CppIR::variable(CppTypeId type, std::string_view name, std::optional<Expr> initializer)
    -> NodeId {
    return variable(type, strings.intern(name), initializer);
}

auto CppIR::variable(std::string_view type, std::string_view name, std::optional<Expr> initializer)
    -> NodeId {
    return variable(types.intern(type), strings.intern(name), initializer);
}

auto CppIR::static_assertion(StringId condition, StringId message) -> NodeId {
    return static_assertion(raw_expr(condition), message);
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

auto CppIR::instance_member(CppTypeId type, StringId name, std::optional<Expr> initializer)
    -> NodeId {
    auto const index{instance_members_.name.size()};
    instance_members_.type.push_back(type);
    instance_members_.name.push_back(name);
    instance_members_.initializer.push_back(initializer.value_or(Expr{}));
    return ir_detail::node(NodeKind::instance_member, index);
}

auto CppIR::static_member(CppTypeId type,
                          StringId name,
                          std::optional<Expr> initializer,
                          bool is_inline,
                          TemplateId template_declaration) -> NodeId {
    auto const index{static_members_.name.size()};
    static_members_.type.push_back(type);
    static_members_.name.push_back(name);
    static_members_.initializer.push_back(initializer.value_or(Expr{}));
    static_members_.template_declaration.push_back(template_declaration);
    static_members_.is_inline.push_back(is_inline);
    return ir_detail::node(NodeKind::static_member, index);
}

auto CppIR::constexpr_member(CppTypeId type,
                             StringId name,
                             Expr initializer,
                             bool is_inline,
                             TemplateId template_declaration) -> NodeId {
    assert(initializer);
    auto const index{constexpr_members_.name.size()};
    constexpr_members_.type.push_back(type);
    constexpr_members_.name.push_back(name);
    constexpr_members_.initializer.push_back(initializer);
    constexpr_members_.template_declaration.push_back(template_declaration);
    constexpr_members_.is_inline.push_back(is_inline);
    return ir_detail::node(NodeKind::constexpr_member, index);
}

auto CppIR::enumerator(StringId name,
                       std::optional<StringId> initializer,
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

auto CppIR::record(StringId name, std::span<NodeId const> children, RecordKind kind) -> RecordId {
    return record(named(name), children, kind);
}

auto CppIR::record(Expr name, std::span<NodeId const> children, RecordKind kind) -> RecordId {
    auto const index{struct_nodes_.name.size()};
    struct_nodes_.name.push_back(name);
    struct_nodes_.children.push_back(save_nodes(children));
    struct_nodes_.bases.push_back({});
    struct_nodes_.template_declaration.push_back({});
    struct_nodes_.requires_clause.push_back({});
    struct_nodes_.export_specifier.push_back({});
    struct_nodes_.record_kind.push_back(kind);
    struct_nodes_.dependencies.push_back({});
    return RecordId{ir_detail::node(NodeKind::struct_, index).value};
}

void CppIR::set_record_bases(RecordId record, std::span<CppTypeId const> bases) {
    struct_nodes_.bases[record.index()] = save_types(bases);
}

void CppIR::set_record_template(RecordId record, TemplateId declaration) {
    struct_nodes_.template_declaration[record.index()] = declaration;
}

void CppIR::set_record_requires_clause(RecordId record, Expr clause) {
    struct_nodes_.requires_clause[record.index()] = clause;
}

void CppIR::set_record_export_specifier(RecordId record, StringId specifier) {
    struct_nodes_.export_specifier[record.index()] = specifier;
}

void CppIR::set_record_dependencies(RecordId record,
                                    std::span<TypeDependencyId const> dependencies) {
    struct_nodes_.dependencies[record.index()] = save_dependencies(dependencies);
}

auto CppIR::namespace_node(StringId name, std::span<NodeId const> children) -> NodeId {
    auto const index{namespace_nodes_.name.size()};
    namespace_nodes_.name.push_back(name);
    namespace_nodes_.children.push_back(save_nodes(children));
    return ir_detail::node(NodeKind::namespace_, index);
}

auto CppIR::function(StringId name, CppTypeId return_type) -> FunctionId {
    return function(named(name), return_type);
}

auto CppIR::function(Expr name, CppTypeId return_type) -> FunctionId {
    assert(return_type.value != 0);
    return append_function(name, return_type, FunctionKind::ordinary);
}

auto CppIR::function(std::string_view name, CppTypeId return_type) -> FunctionId {
    return function(strings.intern(name), return_type);
}

auto CppIR::function(std::string_view name, std::string_view return_type) -> FunctionId {
    return function(strings.intern(name), types.intern(return_type));
}

auto CppIR::append_function(Expr name, CppTypeId return_type, FunctionKind kind) -> FunctionId {
    FunctionId const result{static_cast<std::uint32_t>(functions_.name.size())};
    functions_.name.push_back(name);
    functions_.return_type.push_back(return_type);
    functions_.parameters.push_back({});
    functions_.return_syntax.push_back(ReturnSyntax::direct);
    functions_.kind.push_back(kind);
    functions_.implementation.emplace_back();
    functions_.exception_specification.emplace_back();
    functions_.receiver.push_back(FunctionReceiver::unqualified);
    functions_.flags.push_back({});
    functions_.export_specifier.push_back({});
    functions_.template_declaration.push_back({});
    functions_.requires_clause.push_back({});
    functions_.formatting.push_back({});
    functions_.dependencies.push_back({});
    return result;
}

auto CppIR::constructor(StringId name, bool is_explicit) -> FunctionId {
    return append_function(named(name),
                           {},
                           is_explicit ? FunctionKind::explicit_constructor
                                       : FunctionKind::constructor);
}

auto CppIR::destructor(StringId name) -> FunctionId {
    return append_function(
        named(strings.intern("~" + std::string{strings.view(name)})), {}, FunctionKind::destructor);
}

auto CppIR::parameter(CppTypeId type, StringId name, std::optional<StringId> default_value)
    -> FunctionParameterId {
    FunctionParameterId const result{static_cast<std::uint32_t>(parameters_.name.size())};
    parameters_.type.push_back(type);
    parameters_.name.push_back(name);
    parameters_.default_value.push_back(default_value);
    return result;
}

auto CppIR::parameter(CppTypeId type, std::string_view name, std::optional<StringId> default_value)
    -> FunctionParameterId {
    return parameter(type, strings.intern(name), default_value);
}

auto CppIR::parameter(std::string_view type,
                      std::string_view name,
                      std::optional<StringId> default_value) -> FunctionParameterId {
    return parameter(types.intern(type), strings.intern(name), default_value);
}

auto CppIR::member_initializer(StringId member, StringId value) -> MemberInitializerId {
    return member_initializer(member, raw_expr(value));
}

auto CppIR::member_initializer(StringId member, Expr value) -> MemberInitializerId {
    MemberInitializerId const result{static_cast<std::uint32_t>(initializers_.member.size())};
    initializers_.member.push_back(member);
    initializers_.value.push_back(value);
    return result;
}

void CppIR::set_function_parameters(FunctionId function,
                                    std::span<FunctionParameterId const> parameters) {
    functions_.parameters[function.value] = ir::save_range(parameter_references_, parameters);
}

void CppIR::set_function_body(FunctionId function,
                              std::span<NodeId const> body,
                              std::span<MemberInitializerId const> initializers) {
    auto const index{static_cast<std::uint32_t>(function_bodies_.block.size())};
    function_bodies_.block.push_back(block(body));
    function_bodies_.initializers.push_back(ir::save_range(initializer_references_, initializers));
    functions_.implementation[function.value] = FunctionImplementationId{
        (static_cast<std::uint32_t>(FunctionImplementationKind::body) << 24) | index};
}

void CppIR::set_function_defaulted(FunctionId function) {
    functions_.implementation[function.value] = FunctionImplementationId{
        static_cast<std::uint32_t>(FunctionImplementationKind::defaulted) << 24};
}

void CppIR::set_function_deleted(FunctionId function) {
    functions_.implementation[function.value] = FunctionImplementationId{
        static_cast<std::uint32_t>(FunctionImplementationKind::deleted) << 24};
}

void CppIR::set_function_dependencies(FunctionId function,
                                      std::span<TypeDependencyId const> dependencies) {
    functions_.dependencies[function.value] = save_dependencies(dependencies);
}

void CppIR::set_function_name(FunctionId function, StringId name) {
    set_function_name(function, named(name));
}

void CppIR::set_function_name(FunctionId function, Expr name) {
    functions_.name[function.value] = name;
}

void CppIR::set_function_return_type(FunctionId function, CppTypeId type, ReturnSyntax syntax) {
    assert(type.value != 0);
    functions_.return_type[function.value] = type;
    functions_.return_syntax[function.value] = syntax;
    functions_.kind[function.value] = FunctionKind::ordinary;
}

void CppIR::set_function_noexcept(FunctionId function) {
    functions_.exception_specification[function.value] =
        FunctionExceptionId{static_cast<std::uint32_t>(FunctionExceptionKind::noexcept_) << 24};
}

void CppIR::set_function_may_throw(FunctionId function) {
    functions_.exception_specification[function.value] = {};
}

void CppIR::set_function_conditional_noexcept(FunctionId function, Expr condition) {
    assert(condition);
    auto const index{static_cast<std::uint32_t>(conditional_noexcept_.size())};
    conditional_noexcept_.push_back(condition);
    functions_.exception_specification[function.value] = FunctionExceptionId{
        (static_cast<std::uint32_t>(FunctionExceptionKind::conditional) << 24) | index};
}

void CppIR::set_function_receiver(FunctionId function, FunctionReceiver receiver) {
    functions_.receiver[function.value] = receiver;
}

void CppIR::set_function_export_specifier(FunctionId function, StringId specifier) {
    functions_.export_specifier[function.value] = specifier;
}

void CppIR::set_function_template(FunctionId function, TemplateId declaration) {
    functions_.template_declaration[function.value] = declaration;
}

void CppIR::set_function_requires_clause(FunctionId function, Expr clause) {
    functions_.requires_clause[function.value] = clause;
}

auto CppIR::function_flags(FunctionId function) -> FunctionFlags& {
    return functions_.flags[function.value];
}

auto CppIR::function_formatting(FunctionId function) -> FunctionFormatting& {
    return functions_.formatting[function.value];
}

auto CppIR::function_return_type(FunctionId function) const -> CppTypeId {
    return functions_.return_type[function.value];
}
auto CppIR::function_parameters(FunctionId function) const -> std::span<FunctionParameterId const> {
    return ir::range_values(parameter_references_, functions_.parameters[function.value]);
}

auto CppIR::function_return_syntax(FunctionId function) const -> ReturnSyntax {
    return functions_.return_syntax[function.value];
}

auto CppIR::function_receiver(FunctionId function) const -> FunctionReceiver {
    return functions_.receiver[function.value];
}

auto CppIR::parameter_name(FunctionParameterId parameter) const -> StringId {
    return parameters_.name[parameter.value];
}
auto CppIR::parameter_type(FunctionParameterId parameter) const -> CppTypeId {
    return parameters_.type[parameter.value];
}
void CppIR::set_parameter_type(FunctionParameterId parameter, CppTypeId type) {
    parameters_.type[parameter.value] = type;
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
    if (!declaration &&
        functions_.implementation[function.value].kind() == FunctionImplementationKind::none) {
        set_function_body(function, {});
    }
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
    clear(type_reference_exprs_.type);
    clear(parenthesized_exprs_.operand);
    clear(parameter_references_,
          initializer_references_,
          case_references_,
          enumerator_references_,
          children_,
          arguments_,
          type_references_,
          dependencies_);
    clear(raw_nodes_.text, raw_nodes_.dependencies);
    clear(raw_lines_nodes_.lines, raw_lines_nodes_.dependencies);
    clear(raw_lines_);
    clear(line_comment_nodes_.text);
    clear(block_comment_nodes_.text);
    clear(block_nodes_.children);
    clear(if_stmt_nodes_.condition, if_stmt_nodes_.then_children, if_stmt_nodes_.else_children);
    clear(switch_stmt_nodes_.condition, switch_stmt_nodes_.cases);
    clear(expr_stmt_nodes_.expr, expr_stmt_nodes_.dependencies);
    clear(return_stmt_nodes_.expr, return_stmt_nodes_.dependencies);
    clear(assignment_stmt_nodes_.target,
          assignment_stmt_nodes_.value,
          assignment_stmt_nodes_.dependencies);
    clear(variable_declaration_stmt_nodes_.type,
          variable_declaration_stmt_nodes_.name,
          variable_declaration_stmt_nodes_.initializer);
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
          constexpr_members_.template_declaration,
          constexpr_members_.is_inline);
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
    clear(sizeof_exprs_.type);
    clear(initializer_exprs_.type, initializer_exprs_.elements);
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

auto CppIR::named(std::string_view value, std::span<TypeDependencyId const> dependencies) -> Expr {
    return named(strings.intern(value), dependencies);
}
auto CppIR::named(StringId value, std::span<TypeDependencyId const> dependencies) -> Expr {
    auto const result{expr_detail::handle(ExprKind::named, named_exprs_.spelling.size())};
    named_exprs_.spelling.push_back(value);
    named_exprs_.dependencies.push_back(save_dependencies(dependencies));
    return result;
}
auto CppIR::literal(std::string_view value) -> Expr {
    return literal(strings.intern(value));
}
auto CppIR::literal(StringId value) -> Expr {
    auto const result{expr_detail::handle(ExprKind::literal, literal_exprs_.spelling.size())};
    literal_exprs_.spelling.push_back(value);
    return result;
}
auto CppIR::string_literal(std::string_view value) -> Expr {
    return string_literal(strings.intern(value));
}
auto CppIR::string_literal(StringId value) -> Expr {
    auto const result{expr_detail::handle(ExprKind::string_literal, string_literal_exprs_.size())};
    string_literal_exprs_.push_back(value);
    return result;
}
auto CppIR::raw_expr(std::string_view value, std::span<TypeDependencyId const> dependencies)
    -> Expr {
    return raw_expr(strings.intern(value), dependencies);
}
auto CppIR::raw_expr(StringId value, std::span<TypeDependencyId const> dependencies) -> Expr {
    auto const result{expr_detail::handle(ExprKind::raw, raw_exprs_.text.size())};
    raw_exprs_.text.push_back(value);
    raw_exprs_.dependencies.push_back(save_dependencies(dependencies));
    return result;
}
auto CppIR::call(Expr callee, std::span<Expr const> arguments) -> Expr {
    auto const result{expr_detail::handle(ExprKind::call, call_exprs_.callee.size())};
    call_exprs_.callee.push_back(callee);
    call_exprs_.arguments.push_back(save_exprs(arguments));
    return result;
}
auto CppIR::member_access(Expr object, std::string_view member) -> Expr {
    return member_access(object, strings.intern(member));
}
auto CppIR::member_access(Expr object, StringId member) -> Expr {
    auto const result{expr_detail::handle(ExprKind::member_access, member_exprs_.object.size())};
    member_exprs_.object.push_back(object);
    member_exprs_.member.push_back(member);
    member_exprs_.through_pointer.push_back(false);
    return result;
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
    return result;
}
auto CppIR::subscript(Expr object, Expr index) -> Expr {
    auto const result{expr_detail::handle(ExprKind::subscript, subscript_exprs_.object.size())};
    subscript_exprs_.object.push_back(object);
    subscript_exprs_.index.push_back(index);
    return result;
}
auto CppIR::binary(BinaryOperator operation, Expr left, Expr right, bool line_break_after) -> Expr {
    auto const result{expr_detail::handle(ExprKind::binary, binary_exprs_.operation.size())};
    binary_exprs_.operation.push_back(operation);
    binary_exprs_.left.push_back(left);
    binary_exprs_.right.push_back(right);
    binary_exprs_.line_break_after.push_back(line_break_after);
    return result;
}
auto CppIR::static_cast_expr(CppTypeId type, Expr operand) -> Expr {
    auto const result{expr_detail::handle(ExprKind::static_cast_, cast_exprs_.type.size())};
    cast_exprs_.type.push_back(type);
    cast_exprs_.operand.push_back(operand);
    return result;
}
auto CppIR::sizeof_type(CppTypeId type) -> Expr {
    auto const result{expr_detail::handle(ExprKind::sizeof_type, sizeof_exprs_.type.size())};
    sizeof_exprs_.type.push_back(type);
    return result;
}
auto CppIR::init_list(std::span<Expr const> elements, std::optional<CppTypeId> type) -> Expr {
    auto const result{
        expr_detail::handle(ExprKind::initializer_list, initializer_exprs_.type.size())};
    initializer_exprs_.type.push_back(type);
    initializer_exprs_.elements.push_back(save_exprs(elements));
    return result;
}
auto CppIR::type_reference(CppTypeId type) -> Expr {
    auto const result{
        expr_detail::handle(ExprKind::type_reference, type_reference_exprs_.type.size())};
    type_reference_exprs_.type.push_back(type);
    return result;
}
auto CppIR::parenthesized(Expr expr) -> Expr {
    auto const result{
        expr_detail::handle(ExprKind::parenthesized, parenthesized_exprs_.operand.size())};
    parenthesized_exprs_.operand.push_back(expr);
    return result;
}
auto CppIR::template_id(Expr callee, std::span<Expr const> arguments) -> Expr {
    auto const result{expr_detail::handle(ExprKind::template_id, template_id_exprs_.callee.size())};
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
auto CppIR::value_template_parameter(CppTypeId type, StringId name, Expr default_value, bool pack)
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
            type(variable_declaration_stmt_nodes_.type[i]);
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
            auto const end{range.begin + range.count};
            for (auto c{range.begin}; c < end; ++c) {
                if (auto const label{cases_.label[case_references_[c].value]}) {
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
            auto const end{range.begin + range.count};
            for (auto p{range.begin}; p < end; ++p) {
                type(parameters_.type[parameter_references_[p].value]);
            }
            auto const implementation{functions_.implementation[id]};
            if (!function_nodes_.declaration[i] &&
                implementation.kind() == FunctionImplementationKind::body) {
                auto const initializers{function_bodies_.initializers[implementation.index()]};
                auto const initializers_end{initializers.begin + initializers.count};
                for (auto m{initializers.begin}; m < initializers_end; ++m) {
                    collect_dependencies(initializers_.value[initializer_references_[m].value],
                                         result);
                }
            }
            break;
        }
        case NodeKind::enum_:
            if (enum_nodes_.underlying_type[i].value != 0) {
                type(enum_nodes_.underlying_type[i]);
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
        case ExprKind::type_reference:
            append(types.dependencies(type_reference_exprs_.type[index]));
            break;
        case ExprKind::parenthesized:
            collect_dependencies(parenthesized_exprs_.operand[index], result);
            break;
        case ExprKind::template_id:
            collect_dependencies(template_id_exprs_.callee[index], result);
            for (auto const argument : exprs(template_id_exprs_.arguments[index])) {
                collect_dependencies(argument, result);
            }
            break;
        case ExprKind::named:
            append(dependency_list(named_exprs_.dependencies[index]));
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
            append(types.dependencies(cast_exprs_.type[index]));
            collect_dependencies(cast_exprs_.operand[index], result);
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
        case ExprKind::literal:
        case ExprKind::string_literal:
            break;
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
    auto const end{range.begin + range.count};
    for (auto position{range.begin}; position < end; ++position) {
        auto const parameter{template_parameter_references_[position]};
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

} // namespace lispb::codegen
