#include <codegen/ast/cpp_ir.h>

#include <ioj/is_any.h>

#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_exception_id.h>
#include <codegen/ast/function_exception_kind.h>
#include <codegen/ast/function_implementation_id.h>
#include <codegen/ast/function_implementation_kind.h>
#include <codegen/ast/function_kind.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/node_kind.h>
#include <lispb/strings.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
namespace lispb::codegen {

auto CppIR::store_text(std::string_view value) -> StringId {
    return strings.add(value);
}
auto CppIR::store_optional_text(std::optional<std::string_view> value) -> std::optional<StringId> {
    return value ? std::optional{strings.add(*value)} : std::nullopt;
}

namespace ir_detail {
auto node(NodeKind kind, std::size_t index) -> NodeId {
    assert(index < 0x01000000U);
    return NodeId{(static_cast<std::uint32_t>(kind) << 24) | static_cast<std::uint32_t>(index)};
}
}

auto CppIR::save_nodes(std::span<NodeId const> values) -> ir::Range {
    ir::Range const range{static_cast<std::uint32_t>(children_.size()),
                          static_cast<std::uint32_t>(values.size())};
    children_.insert(children_.end(), values.begin(), values.end());
    return range;
}
auto CppIR::nodes(ir::Range range) const -> std::span<NodeId const> {
    return std::span{children_}.subspan(range.begin, range.count);
}
auto CppIR::save_exprs(std::span<Expr const> values) -> ir::Range {
    ir::Range const range{static_cast<std::uint32_t>(arguments_.size()),
                          static_cast<std::uint32_t>(values.size())};
    arguments_.insert(arguments_.end(), values.begin(), values.end());
    return range;
}
auto CppIR::exprs(ir::Range range) const -> std::span<Expr const> {
    return std::span{arguments_}.subspan(range.begin, range.count);
}
auto CppIR::save_types(std::span<CppTypeId const> values) -> ir::Range {
    ir::Range const range{static_cast<std::uint32_t>(type_references_.size()),
                          static_cast<std::uint32_t>(values.size())};
    type_references_.insert(type_references_.end(), values.begin(), values.end());
    return range;
}
auto CppIR::type_list(ir::Range range) const -> std::span<CppTypeId const> {
    return std::span{type_references_}.subspan(range.begin, range.count);
}
auto CppIR::save_dependencies(std::span<TypeDependencyId const> values) -> ir::Range {
    ir::Range const range{static_cast<std::uint32_t>(dependencies_.size()),
                          static_cast<std::uint32_t>(values.size())};
    dependencies_.insert(dependencies_.end(), values.begin(), values.end());
    return range;
}
auto CppIR::dependency_list(ir::Range range) const -> std::span<TypeDependencyId const> {
    return std::span{dependencies_}.subspan(range.begin, range.count);
}
auto CppIR::line_comment(StringId value) -> NodeId {
    auto const index{line_comment_nodes_.text.size()};
    line_comment_nodes_.text.push_back(value);
    return ir_detail::node(NodeKind::line_comment, index);
}

auto CppIR::block_comment(StringId value) -> NodeId {
    auto const index{block_comment_nodes_.text.size()};
    block_comment_nodes_.text.push_back(value);
    return ir_detail::node(NodeKind::block_comment, index);
}

auto CppIR::line_comment(std::string_view value) -> NodeId {
    return line_comment(strings.add(value));
}

auto CppIR::block_comment(std::string_view value) -> NodeId {
    return block_comment(strings.add(value));
}

auto CppIR::line_directive(std::uint32_t line, std::string_view path) -> NodeId {
    auto const index{line_directive_nodes_.line.size()};
    line_directive_nodes_.line.push_back(line);
    line_directive_nodes_.path.push_back(strings.add(path));
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

auto CppIR::switch_case(std::optional<Expr> label, BlockId body) -> SwitchCaseId {
    SwitchCaseId const result{static_cast<std::uint32_t>(cases_.label.size())};
    cases_.label.push_back(label);
    cases_.body.push_back(block_nodes_.children[body.index()]);
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
    switch_stmt_nodes_.cases.push_back(
        ir::Range{static_cast<std::uint32_t>(case_references_.size()),
                  static_cast<std::uint32_t>(cases.size())});
    case_references_.insert(case_references_.end(), cases.begin(), cases.end());
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

auto CppIR::variable(CppTypeId type, IdentifierId name, std::optional<Expr> initializer) -> NodeId {
    auto const index{variable_declaration_stmt_nodes_.type.size()};
    variable_declaration_stmt_nodes_.type.push_back(type);
    variable_declaration_stmt_nodes_.name.push_back(string_id(name));
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

auto CppIR::forward_declaration(IdentifierId name, RecordKind kind, TemplateId declaration)
    -> NodeId {
    auto const index{forward_declaration_nodes_.name.size()};
    forward_declaration_nodes_.name.push_back(string_id(name));
    forward_declaration_nodes_.kind.push_back(kind);
    forward_declaration_nodes_.template_declaration.push_back(declaration);
    return ir_detail::node(NodeKind::forward_declaration, index);
}

auto CppIR::friend_declaration(IdentifierId name, RecordKind kind, TemplateId declaration)
    -> NodeId {
    auto const index{friend_declaration_nodes_.name.size()};
    friend_declaration_nodes_.name.push_back(string_id(name));
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

auto CppIR::using_declaration(IdentifierId name, CppTypeId type, TemplateId declaration) -> NodeId {
    auto const index{using_declaration_nodes_.name.size()};
    using_declaration_nodes_.name.push_back(string_id(name));
    using_declaration_nodes_.type.push_back(type);
    using_declaration_nodes_.template_declaration.push_back(declaration);
    return ir_detail::node(NodeKind::using_declaration, index);
}

auto CppIR::instance_member(CppTypeId type, IdentifierId name, std::optional<Expr> initializer)
    -> NodeId {
    auto const index{instance_members_.name.size()};
    instance_members_.type.push_back(type);
    instance_members_.name.push_back(name);
    instance_members_.initializer.push_back(initializer.value_or(Expr{}));
    return ir_detail::node(NodeKind::instance_member, index);
}

auto CppIR::static_member(CppTypeId type,
                          IdentifierId name,
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
                             IdentifierId name,
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

auto CppIR::enumerator(IdentifierId name,
                       std::optional<StringId> initializer,
                       std::optional<StringId> annotation) -> EnumValueId {
    EnumValueId const result{static_cast<std::uint32_t>(enumerators_.name.size())};
    enumerators_.name.push_back(string_id(name));
    enumerators_.initializer.push_back(initializer);
    enumerators_.annotation.push_back(annotation);
    return result;
}

auto CppIR::enumeration(IdentifierId name,
                        CppTypeId underlying_type,
                        std::span<EnumValueId const> values,
                        IdentifierId export_specifier,
                        bool scoped) -> NodeId {
    auto const index{enum_nodes_.name.size()};
    enum_nodes_.name.push_back(string_id(name));
    enum_nodes_.underlying_type.push_back(underlying_type);
    enum_nodes_.values.push_back(
        ir::Range{static_cast<std::uint32_t>(enumerator_references_.size()),
                  static_cast<std::uint32_t>(values.size())});
    enum_nodes_.export_specifier.push_back(export_specifier);
    enum_nodes_.scoped.push_back(scoped);
    enumerator_references_.insert(enumerator_references_.end(), values.begin(), values.end());
    return ir_detail::node(NodeKind::enum_, index);
}

auto CppIR::record(IdentifierId name, std::span<NodeId const> children, RecordKind kind)
    -> RecordId {
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

void CppIR::set_record_bases(RecordId record, std::span<CppTypeId const> value) {
    struct_nodes_.bases[record.index()] = save_types(value);
}

void CppIR::set_record_template(RecordId record, TemplateId value) {
    struct_nodes_.template_declaration[record.index()] = value;
}

void CppIR::set_record_requires_clause(RecordId record, Expr value) {
    struct_nodes_.requires_clause[record.index()] = value;
}

void CppIR::set_record_export_specifier(RecordId record, IdentifierId value) {
    struct_nodes_.export_specifier[record.index()] = value;
}

void CppIR::set_record_dependencies(RecordId record, std::span<TypeDependencyId const> value) {
    struct_nodes_.dependencies[record.index()] = save_dependencies(value);
}

auto CppIR::namespace_node(IdentifierId name, std::span<NodeId const> children) -> NodeId {
    auto const index{namespace_nodes_.name.size()};
    namespace_nodes_.name.push_back(string_id(name));
    namespace_nodes_.children.push_back(save_nodes(children));
    return ir_detail::node(NodeKind::namespace_, index);
}

auto CppIR::function(IdentifierId name, CppTypeId return_type) -> FunctionId {
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

auto CppIR::constructor(IdentifierId name, bool is_explicit) -> FunctionId {
    return append_function(named(name),
                           {},
                           is_explicit ? FunctionKind::explicit_constructor
                                       : FunctionKind::constructor);
}

auto CppIR::destructor(IdentifierId name) -> FunctionId {
    return append_function(
        named(strings.intern("~" + std::string{strings.view(name)})), {}, FunctionKind::destructor);
}

auto CppIR::parameter(CppTypeId type, IdentifierId name, std::optional<StringId> default_value)
    -> FunctionParameterId {
    FunctionParameterId const result{static_cast<std::uint32_t>(parameters_.name.size())};
    parameters_.type.push_back(type);
    parameters_.name.push_back(string_id(name));
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

auto CppIR::member_initializer(IdentifierId member, StringId value) -> MemberInitializerId {
    MemberInitializerId const result{static_cast<std::uint32_t>(initializers_.member.size())};
    initializers_.member.push_back(string_id(member));
    initializers_.value.push_back(value);
    return result;
}

void CppIR::set_function_parameters(FunctionId function,
                                    std::span<FunctionParameterId const> values) {
    functions_.parameters[function.value] = {
        static_cast<std::uint32_t>(parameter_references_.size()),
        static_cast<std::uint32_t>(values.size())};
    parameter_references_.insert(parameter_references_.end(), values.begin(), values.end());
}

void CppIR::set_function_body(FunctionId function,
                              std::span<NodeId const> body,
                              std::span<MemberInitializerId const> initializers) {
    auto const index{static_cast<std::uint32_t>(function_bodies_.block.size())};
    function_bodies_.block.push_back(block(body));
    function_bodies_.initializers.push_back(
        {static_cast<std::uint32_t>(initializer_references_.size()),
         static_cast<std::uint32_t>(initializers.size())});
    initializer_references_.insert(
        initializer_references_.end(), initializers.begin(), initializers.end());
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
                                      std::span<TypeDependencyId const> value) {
    functions_.dependencies[function.value] = save_dependencies(value);
}

void CppIR::set_function_name(FunctionId function, IdentifierId value) {
    set_function_name(function, named(value));
}

void CppIR::set_function_name(FunctionId function, Expr value) {
    functions_.name[function.value] = value;
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

void CppIR::set_function_export_specifier(FunctionId function, IdentifierId value) {
    functions_.export_specifier[function.value] = value;
}

void CppIR::set_function_template(FunctionId function, TemplateId value) {
    functions_.template_declaration[function.value] = value;
}

void CppIR::set_function_requires_clause(FunctionId function, Expr value) {
    functions_.requires_clause[function.value] = value;
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
    auto const range{functions_.parameters[function.value]};
    return std::span{parameter_references_}.subspan(range.begin, range.count);
}

auto CppIR::function_return_syntax(FunctionId function) const -> ReturnSyntax {
    return functions_.return_syntax[function.value];
}

auto CppIR::function_receiver(FunctionId function) const -> FunctionReceiver {
    return functions_.receiver[function.value];
}

auto CppIR::parameter_name(FunctionParameterId parameter) const -> IdentifierId {
    return IdentifierId{parameters_.name[parameter.value].value};
}
auto CppIR::parameter_type(FunctionParameterId parameter) const -> CppTypeId {
    return parameters_.type[parameter.value];
}
void CppIR::set_parameter_type(FunctionParameterId parameter, CppTypeId type) {
    parameters_.type[parameter.value] = type;
}

auto CppIR::function_node(FunctionId function,
                          std::optional<StringId> owner,
                          bool declaration,
                          bool header,
                          std::optional<ReturnSyntax> return_syntax) -> NodeId {
    if (owner &&
        functions_.implementation[function.value].kind() == FunctionImplementationKind::deleted) {
        throw std::invalid_argument{"Function '" + render(functions_.name[function.value]) +
                                    "' deleted function cannot be an out-of-line definition"};
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
                       std::optional<StringId> owner,
                       std::optional<ReturnSyntax> syntax,
                       bool header) -> NodeId {
    return function_node(function, owner, false, header, syntax);
}
void CppIR::clear() {
    line_directive_nodes_.line.clear();
    line_directive_nodes_.path.clear();
    for_stmt_nodes_.initializer.clear();
    for_stmt_nodes_.condition.clear();
    for_stmt_nodes_.increment.clear();
    for_stmt_nodes_.children.clear();
    files.clear();
    modules.clear();
    lowering.clear();
    template_parameter_references_.clear();
    type_template_parameters_.name.clear();
    type_template_parameters_.default_type.clear();
    type_template_parameters_.pack.clear();
    value_template_parameters_.type.clear();
    value_template_parameters_.name.clear();
    value_template_parameters_.default_value.clear();
    value_template_parameters_.pack.clear();
    templates_.parameters.clear();
    templates_.authored_text.clear();
    template_id_exprs_.callee.clear();
    template_id_exprs_.arguments.clear();
    type_reference_exprs_.type.clear();
    parenthesized_exprs_.operand.clear();

    parameter_references_.clear();
    initializer_references_.clear();
    case_references_.clear();
    enumerator_references_.clear();
    children_.clear();
    arguments_.clear();
    type_references_.clear();
    dependencies_.clear();
    raw_nodes_.text.clear();
    raw_nodes_.dependencies.clear();
    raw_lines_nodes_.lines.clear();
    raw_lines_nodes_.dependencies.clear();
    raw_lines_.clear();
    line_comment_nodes_.text.clear();
    block_comment_nodes_.text.clear();
    block_nodes_.children.clear();
    if_stmt_nodes_.condition.clear();
    if_stmt_nodes_.then_children.clear();
    if_stmt_nodes_.else_children.clear();
    switch_stmt_nodes_.condition.clear();
    switch_stmt_nodes_.cases.clear();
    expr_stmt_nodes_.expr.clear();
    expr_stmt_nodes_.dependencies.clear();
    return_stmt_nodes_.expr.clear();
    return_stmt_nodes_.dependencies.clear();
    assignment_stmt_nodes_.target.clear();
    assignment_stmt_nodes_.value.clear();
    assignment_stmt_nodes_.dependencies.clear();
    variable_declaration_stmt_nodes_.type.clear();
    variable_declaration_stmt_nodes_.name.clear();
    variable_declaration_stmt_nodes_.initializer.clear();
    static_assert_nodes_.condition.clear();
    static_assert_nodes_.message.clear();
    new_lines_nodes_.count.clear();
    access_specifier_nodes_.access.clear();
    access_specifier_nodes_.indentation.clear();
    include_nodes_.path.clear();
    include_nodes_.style.clear();
    forward_declaration_nodes_.name.clear();
    forward_declaration_nodes_.kind.clear();
    forward_declaration_nodes_.template_declaration.clear();
    friend_declaration_nodes_.name.clear();
    friend_declaration_nodes_.kind.clear();
    friend_declaration_nodes_.type.clear();
    friend_declaration_nodes_.template_declaration.clear();
    using_declaration_nodes_.name.clear();
    using_declaration_nodes_.type.clear();
    using_declaration_nodes_.template_declaration.clear();
    instance_members_.type.clear();
    instance_members_.name.clear();
    instance_members_.initializer.clear();
    static_members_.type.clear();
    static_members_.name.clear();
    static_members_.initializer.clear();
    static_members_.template_declaration.clear();
    static_members_.is_inline.clear();
    constexpr_members_.type.clear();
    constexpr_members_.name.clear();
    constexpr_members_.initializer.clear();
    constexpr_members_.template_declaration.clear();
    constexpr_members_.is_inline.clear();
    function_nodes_.function.clear();
    function_nodes_.owner.clear();
    function_nodes_.declaration.clear();
    function_nodes_.is_header.clear();
    function_nodes_.return_syntax.clear();
    enum_nodes_.name.clear();
    enum_nodes_.underlying_type.clear();
    enum_nodes_.values.clear();
    enum_nodes_.export_specifier.clear();
    enum_nodes_.scoped.clear();
    struct_nodes_.name.clear();
    struct_nodes_.children.clear();
    struct_nodes_.bases.clear();
    struct_nodes_.template_declaration.clear();
    struct_nodes_.requires_clause.clear();
    struct_nodes_.export_specifier.clear();
    struct_nodes_.record_kind.clear();
    struct_nodes_.dependencies.clear();
    namespace_nodes_.name.clear();
    namespace_nodes_.children.clear();
    named_exprs_.spelling.clear();
    named_exprs_.dependencies.clear();
    literal_exprs_.spelling.clear();
    string_literal_exprs_.clear();
    call_exprs_.callee.clear();
    call_exprs_.arguments.clear();
    member_exprs_.object.clear();
    member_exprs_.member.clear();
    member_exprs_.through_pointer.clear();
    subscript_exprs_.object.clear();
    subscript_exprs_.index.clear();
    binary_exprs_.operation.clear();
    binary_exprs_.left.clear();
    binary_exprs_.right.clear();
    binary_exprs_.line_break_after.clear();
    unary_exprs_.operation.clear();
    unary_exprs_.operand.clear();
    cast_exprs_.type.clear();
    cast_exprs_.operand.clear();
    sizeof_exprs_.type.clear();
    initializer_exprs_.type.clear();
    initializer_exprs_.elements.clear();
    raw_exprs_.text.clear();
    raw_exprs_.dependencies.clear();
    parameters_.type.clear();
    parameters_.name.clear();
    parameters_.default_value.clear();
    initializers_.member.clear();
    initializers_.value.clear();
    enumerators_.name.clear();
    enumerators_.initializer.clear();
    enumerators_.annotation.clear();
    cases_.label.clear();
    cases_.body.clear();
    functions_.name.clear();
    functions_.return_type.clear();
    functions_.parameters.clear();
    functions_.return_syntax.clear();
    functions_.kind.clear();
    functions_.implementation.clear();
    functions_.exception_specification.clear();
    functions_.receiver.clear();
    function_bodies_.block.clear();
    function_bodies_.initializers.clear();
    conditional_noexcept_.clear();
    functions_.flags.clear();
    functions_.export_specifier.clear();
    functions_.template_declaration.clear();
    functions_.requires_clause.clear();
    functions_.formatting.clear();
    functions_.dependencies.clear();
}

auto CppIR::raw(std::string_view value, std::span<TypeDependencyId const> dependencies) -> NodeId {
    return raw(strings.add(value), dependencies);
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
    raw_lines_nodes_.lines.push_back(
        {static_cast<std::uint32_t>(raw_lines_.size()), static_cast<std::uint32_t>(lines.size())});
    raw_lines_nodes_.dependencies.push_back(save_dependencies(dependencies));
    raw_lines_.insert(raw_lines_.end(), lines.begin(), lines.end());
    return ir_detail::node(NodeKind::raw_lines, index);
}

} // namespace lispb::codegen
