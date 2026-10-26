#include <codegen/ast/cpp_renderer.h>

#include <ioj/is_any.h>

#include <codegen/ast/access.h>
#include <codegen/ast/access_indentation.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/environment_guard.h>
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
#include <format>
#include <functional>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {

namespace environment_detail {
auto components(Strings& strings, std::string const& name) -> std::vector<StringId> {
    // Borrow an owned spelling because interning can move the Strings buffer.
    std::vector<StringId> result;
    std::string_view remaining{name};
    if (remaining.starts_with("::")) {
        remaining.remove_prefix(2);
    }
    while (!remaining.empty()) {
        auto const end{remaining.find("::")};
        result.push_back(strings.intern(remaining.substr(0, end)));
        if (end == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(end + 2);
    }
    return result;
}
auto untemplated(std::string_view name) -> std::string {
    std::string result;
    unsigned depth{};
    for (auto const character : name) {
        if (character == '<') {
            ++depth;
        } else if (character == '>' && depth != 0) {
            --depth;
        } else if (depth == 0) {
            result += character;
        }
    }
    return result;
}
}

namespace expr_detail {
auto binary_precedence(BinaryOperator operation) -> int {
    switch (operation) {
        case BinaryOperator::logical_or:
            return 1;
        case BinaryOperator::logical_and:
            return 2;
        case BinaryOperator::bitwise_or:
            return 3;
        case BinaryOperator::bitwise_and:
            return 4;
        case BinaryOperator::equal:
        case BinaryOperator::not_equal:
            return 5;
        case BinaryOperator::greater_equal:
        case BinaryOperator::greater:
        case BinaryOperator::less:
        case BinaryOperator::less_equal:
            return 6;
        case BinaryOperator::three_way:
            return 7;
        case BinaryOperator::shift_left:
            return 8;
        case BinaryOperator::subtract:
        case BinaryOperator::add:
            return 9;
        case BinaryOperator::multiply:
        case BinaryOperator::divide:
            return 10;
    }
    std::unreachable();
}
auto binary_spelling(BinaryOperator operation) -> std::string_view {
    switch (operation) {
        case BinaryOperator::logical_or:
            return " || ";
        case BinaryOperator::logical_and:
            return " && ";
        case BinaryOperator::equal:
            return " == ";
        case BinaryOperator::not_equal:
            return " != ";
        case BinaryOperator::greater_equal:
            return " >= ";
        case BinaryOperator::greater:
            return " > ";
        case BinaryOperator::less:
            return " < ";
        case BinaryOperator::less_equal:
            return " <= ";
        case BinaryOperator::three_way:
            return " <=> ";
        case BinaryOperator::subtract:
            return " - ";
        case BinaryOperator::add:
            return " + ";
        case BinaryOperator::multiply:
            return " * ";
        case BinaryOperator::divide:
            return " / ";
        case BinaryOperator::bitwise_and:
            return " & ";
        case BinaryOperator::bitwise_or:
            return " | ";
        case BinaryOperator::shift_left:
            return " << ";
    }
    std::unreachable();
}
}

/* **************************************** */
// Entry points
/* **************************************** */
CppRenderer::CppRenderer(CppIR const& ir, std::string_view indent_text)
    : ir_{ir}
    , indent_text_{indent_text} {}
auto CppRenderer::render(Expr expr, RenderContext const& context) -> std::string {
    state_.clear();
    return expression(expr, context);
}
auto CppRenderer::render(NodeId node, RenderContext const& context) -> std::string {
    state_.clear();
    initialize_environments(std::span{&node, 1});
    std::string output;
    append_node(output, node, context);
    return output;
}
auto CppRenderer::render(CppFileId file) -> std::string {
    state_.clear();
    auto const row{ir_.files.index(file)};
    auto const file_nodes{ir_.files.nodes(file)};
    auto const flags{ir_.files.flags[row]};
    std::vector<TypeDependencyId> dependencies;
    for (auto const node : file_nodes) {
        ir_.collect_dependencies(node, dependencies);
    }
    std::set<std::string> paths;
    for (auto const dependency : dependencies) {
        if (auto const header{ir_.types.dependency_header(dependency)}) {
            paths.emplace(*header);
        }
    }
    std::vector<std::vector<std::string>> include_groups;
    for (auto const prefix_id : ir_.files.include_order(file)) {
        auto const prefix{ir_.strings.view(prefix_id)};
        std::vector<std::string> group;
        for (auto it{paths.begin()}; it != paths.end();) {
            if (it->starts_with(prefix)) {
                group.push_back(*it);
                it = paths.erase(it);
            } else {
                ++it;
            }
        }
        if (!group.empty()) {
            include_groups.push_back(std::move(group));
        }
    }
    std::vector<std::string> quoted;
    std::vector<std::string> system;
    for (auto const& path : paths) {
        (include_is_system(path) ? system : quoted).push_back(path);
    }
    if (!quoted.empty()) {
        include_groups.push_back(std::move(quoted));
    }
    if (!system.empty()) {
        include_groups.push_back(std::move(system));
    }
    initialize_environments(file_nodes);
    RenderContext const context{.include_groups = include_groups};
    std::string output;
    if (flags.clang_format_off()) {
        output += "// clang-format off\n";
    }
    output += "// This file is autogenerated. Do not edit by hand.\n";
    if (ir_.files.source[row].value != 0) {
        output += "// Source: ";
        output += ir_.strings.view(ir_.files.source[row]);
    } else {
        output += "// Edit lispb/schema and regenerate it instead.";
    }
    for (auto const prologue : ir_.files.prologue(file)) {
        output += "\n\n";
        output += ir_.strings.view(prologue);
    }
    if (flags.pragma_once()) {
        output += "\n\n#pragma once";
    }
    if (!file_nodes.empty()) {
        output += "\n\n";
        append_nodes(output, file_nodes, context, 1);
    }
    for (auto const epilogue : ir_.files.epilogue(file)) {
        output += "\n\n";
        output += ir_.strings.view(epilogue);
    }
    while (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }
    output += '\n';
    if (flags.clang_format_off()) {
        output += "// clang-format on\n";
    }
    return output;
}
auto CppRenderer::render_nodes(std::span<NodeId const> values,
                               RenderContext const& context,
                               int newlines) -> std::string {
    state_.clear();
    initialize_environments(values);
    std::string output;
    append_nodes(output, values, context, newlines);
    return output;
}

/* **************************************** */
// Formatting
/* **************************************** */
void CppRenderer::append_indent(std::string& output, RenderContext const& context) const {
    for (int level{}; level < context.indent_level; ++level) {
        output += indent_text_;
    }
}
void CppRenderer::append_indented(std::string& output,
                                  std::string_view text,
                                  RenderContext const& context) const {
    bool line_start{true};
    for (auto const character : text) {
        if (line_start && character != '\n') {
            append_indent(output, context);
        }
        output += character;
        line_start = character == '\n';
    }
}

/* **************************************** */
// Types
auto CppRenderer::in_record_scope(EnvironmentId environment) const -> bool {
    while (state_.environments.kind(environment) == EnvironmentKind::template_) {
        environment = state_.environments.parent(environment);
    }
    return state_.environments.kind(environment) == EnvironmentKind::record;
}

/* **************************************** */
// Types and environments
/* **************************************** */
void CppRenderer::append_type(std::string& output, CppTypeId const type) {
    assert(type.value != 0 && type.value <= ir_.types.shapes_.size());
    auto const shape{ir_.types.shapes_[type.value - 1]};
    if (shape.form == CppTypeForm::raw) {
        output += ir_.types.spelling(type);
    } else if (shape.form == CppTypeForm::named) {
        if (shape.parts.count == 1) {
            output += ir_.types.spelling(type);
            return;
        }
        std::vector<StringId> components;
        for (auto const component :
             std::span{ir_.types.name_components_}.subspan(shape.parts.begin, shape.parts.count)) {
            components.push_back(component);
        }
        append_name(output, components);
    } else {
        for (auto const part :
             std::span{ir_.types.parts_}.subspan(shape.parts.begin, shape.parts.count)) {
            if (part.type.value != 0) {
                append_type(output, part.type);
            } else {
                output += ir_.strings.view(part.text);
            }
        }
    }
}
void CppRenderer::append_name(std::string& output, std::span<StringId const> components) {
    std::string canonical;
    for (auto const component : components) {
        if (!canonical.empty()) {
            canonical += "::";
        }
        canonical += ir_.strings.view(component);
    }
    auto const target{state_.environments.reference(components)};

    auto const count{components.size()};
    for (auto first{count - 1};; --first) {
        auto const result{state_.environments.lookup(state_.current, components.subspan(first))};
        if (result.status == LookupStatus::found && result.symbol.same_declaration(target)) {
            for (auto component{first}; component < count; ++component) {
                if (component != first) {
                    output += "::";
                }
                output += ir_.strings.view(components[component]);
            }
            return;
        }
        if (first == 0) {
            // Preserve authored qualification when the complete name cannot be checked.
            if (result.status != LookupStatus::unknown) {
                output += "::";
            }
            output += canonical;
            return;
        }
    }
}

/* **************************************** */
// Environments, expressions and nodes
auto CppRenderer::declaration_name(Expr expression) -> StringId {
    while (expression.kind() == ExprKind::template_id) {
        expression = ir_.template_id_exprs_.callee[expression.index()];
    }
    if (expression.kind() != ExprKind::named) {
        return {};
    }
    auto const text{ir_.strings.view(ir_.named_exprs_.spelling[expression.index()])};
    return ir_.strings.intern(text.substr(0, text.find_first_of("<:")));
}
void CppRenderer::collect_environment(EnvironmentId environment,
                                      std::span<NodeId const> children,
                                      bool bindings_known) {
    auto& state{state_};
    auto& environments{state.environments};
    for (auto const node : children) {
        auto const i{node.index()};
        auto add_name = [&](auto name, CppDeclarationId declaration) {
            std::string const text{ir_.strings.view(name)};
            if (!text.empty()) {
                environments.declare(environment,
                                     ir_.strings.intern(text.substr(0, text.find_first_of("<:"))),
                                     declaration);
            }
        };
        auto add_expr = [&](Expr expression, CppDeclarationId declaration) {
            auto const name{declaration_name(expression)};
            if (name.value != 0) {
                environments.declare(environment, name, declaration);
            } else {
                environments.mark_unknown(environment);
            }
        };
        auto const declaration{CppDeclarationId{node}};
        switch (node.kind()) {
            case NodeKind::namespace_: {
                std::string const name{ir_.strings.view(ir_.namespace_nodes_.name[i])};
                auto nested{environment};
                if (name.empty()) {
                    nested = environments.append(environment, EnvironmentKind::namespace_);
                } else {
                    for (auto const component : environment_detail::components(ir_.strings, name)) {
                        nested = environments.named(nested, EnvironmentKind::namespace_, component);
                    }
                }
                state.node_environments[RenderState::node_key(node, environment)] = nested;
                collect_environment(nested, ir_.nodes(ir_.namespace_nodes_.children[i]));
                if (name.empty()) {
                    environments.import_symbols(environment, nested);
                }
                break;
            }
            case NodeKind::struct_: {
                auto const name{declaration_name(ir_.struct_nodes_.name[i])};
                auto nested{EnvironmentId{}};
                if (name.value == 0) {
                    environments.mark_unknown(environment);
                    nested = environments.append(environment, EnvironmentKind::record);
                    environments.mark_unknown(nested);
                } else {
                    nested =
                        environments.named(environment, EnvironmentKind::record, name, declaration);
                    auto const symbol{environments.declare(environment, name, declaration, nested)};
                    environments.bind(nested, name, symbol);
                }
                state.node_environments[RenderState::node_key(node, environment)] = nested;
                collect_template_parameters(nested, ir_.struct_nodes_.template_declaration[i]);
                auto const known{ir_.scope_bindings_.contains(node.value)};
                collect_environment(nested, ir_.nodes(ir_.struct_nodes_.children[i]), known);
                if (!known && ir_.struct_nodes_.bases[i].count != 0) {
                    environments.mark_unknown(nested);
                }
                if (known) {
                    for (auto const binding : ir_.scope_bindings_.at(node.value)) {
                        environments.declare(nested, binding, declaration);
                    }
                }
                break;
            }
            case NodeKind::enum_: {
                auto const name{ir_.strings.intern(ir_.strings.view(ir_.enum_nodes_.name[i]))};
                auto const nested{
                    environments.named(environment, EnvironmentKind::record, name, declaration)};
                auto const range{ir_.enum_nodes_.values[i]};
                auto const end{range.begin + range.count};
                for (auto index{range.begin}; index < end; ++index) {
                    auto const enumerator{ir_.enumerator_references_[index]};
                    environments.declare(
                        nested,
                        ir_.strings.intern(ir_.enumerators_.name[enumerator.value]),
                        declaration);
                }
                if (!ir_.enum_nodes_.scoped[i]) {
                    environments.import_symbols(environment, nested);
                }
                break;
            }
            case NodeKind::forward_declaration:
                add_name(ir_.forward_declaration_nodes_.name[i], declaration);
                break;
            case NodeKind::using_declaration:
                if (ir_.using_declaration_nodes_.name[i].value != 0) {
                    add_name(ir_.using_declaration_nodes_.name[i], declaration);
                } else {
                    auto const type{ir_.using_declaration_nodes_.type[i]};
                    auto const shape{ir_.types.shapes_[type.value - 1]};
                    assert(shape.form == CppTypeForm::named && shape.parts.count > 1);
                    auto const components{std::span{ir_.types.name_components_}.subspan(
                        shape.parts.begin, shape.parts.count)};
                    auto const target{environments.reference(components)};
                    environments.bind(environment, components.back(), target);
                }
                break;
            case NodeKind::instance_member:
                add_name(ir_.instance_members_.name[i], declaration);
                break;
            case NodeKind::static_member:
                add_name(ir_.static_members_.name[i], declaration);
                break;
            case NodeKind::constexpr_member:
                add_name(ir_.constexpr_members_.name[i], declaration);
                break;
            case NodeKind::function:
                if (!ir_.function_nodes_.owner[i]) {
                    add_expr(ir_.functions_.name[ir_.function_nodes_.function[i].value],
                             CppDeclarationId{ir_.function_nodes_.function[i]});
                }
                break;
            case NodeKind::raw:
            case NodeKind::raw_lines:
                if (!bindings_known && !ir_.scope_bindings_.contains(node.value)) {
                    auto const text{node.kind() == NodeKind::raw
                                        ? ir_.strings.view(ir_.raw_nodes_.text[i])
                                        : std::string_view{}};
                    if ((!text.empty() && !text.starts_with("#include ")) ||
                        node.kind() == NodeKind::raw_lines) {
                        environments.mark_unknown(environment);
                    }
                }
                break;
            default:
                break;
        }
        if (node.kind() != NodeKind::struct_) {
            if (auto const extra{ir_.scope_bindings_.find(node.value)};
                extra != ir_.scope_bindings_.end()) {
                for (auto const name : extra->second) {
                    add_name(name, declaration);
                }
            }
        }
    }
}
void CppRenderer::collect_template_parameters(EnvironmentId environment, TemplateId declaration) {
    auto& state{state_};
    if (!declaration || declaration.kind() == TemplateKind::explicit_specialization) {
        return;
    }
    if (declaration.kind() == TemplateKind::authored_raw) {
        state.environments.mark_unknown(environment);
        return;
    }

    auto const range{ir_.templates_.parameters[declaration.index()]};
    auto const end{range.begin + range.count};
    for (auto index{range.begin}; index < end; ++index) {
        auto const parameter{ir_.template_parameter_references_[index]};
        auto const name{parameter.kind() == TemplateParameterKind::type
                            ? ir_.type_template_parameters_.name[parameter.index()]
                            : ir_.value_template_parameters_.name[parameter.index()]};
        if (name.value != 0) {
            state.environments.declare(environment, name, CppDeclarationId{parameter});
        }
    }
}
auto CppRenderer::block_environment(std::span<NodeId const> children, EnvironmentKind kind)
    -> EnvironmentId {
    auto& state{state_};
    auto const environment{state.environments.append(state.current, kind)};
    collect_environment(environment, children);
    return environment;
}
auto CppRenderer::record_environment(std::string_view owner) -> EnvironmentId {
    auto& state{state_};
    auto const name{environment_detail::untemplated(owner)};
    auto const components{environment_detail::components(ir_.strings, name)};
    auto const parent{name.starts_with("::") ? state.environments.global() : state.current};
    auto const found{state.environments.find_environment(parent, components)};
    if (!found.is_null()) {
        return found;
    }

    auto const unknown{state.environments.append(state.current, EnvironmentKind::record)};
    state.environments.mark_unknown(unknown);
    return unknown;
}
void CppRenderer::initialize_environments(std::span<NodeId const> children) {
    auto& state{state_};
    collect_environment(state.environments.global(), children);
}

/* **************************************** */
// Expressions and declarations
/* **************************************** */
auto CppRenderer::expression(Expr expr, RenderContext const& context) -> std::string {
    std::string output;
    append_expr(output, expr, context);
    return output;
}
auto CppRenderer::expr_precedence(Expr expr) -> int {
    switch (expr.kind()) {
        case ExprKind::raw:
            return 0;
        case ExprKind::binary:
            return expr_detail::binary_precedence(ir_.binary_exprs_.operation[expr.index()]);
        case ExprKind::sizeof_type:
        case ExprKind::unary:
            return 11;
        default:
            return 12;
    }
}
void CppRenderer::append_expr(
    std::string& output, Expr expr, RenderContext const& context, int minimum, bool equal) {
    auto const index{expr.index()};
    auto const precedence{expr_precedence(expr)};
    auto const parenthesize{precedence < minimum || (equal && precedence == minimum)};
    if (parenthesize) {
        output += '(';
    }
    auto append_arguments = [&](ir::Range range) {
        bool first{true};
        for (auto const argument : ir_.exprs(range)) {
            if (!first) {
                output += ", ";
            }
            append_expr(output, argument, context, 1);
            first = false;
        }
    };
    switch (expr.kind()) {
        case ExprKind::type_reference:
            append_type(output, ir_.type_reference_exprs_.type[index]);
            break;
        case ExprKind::parenthesized:
            output += '(';
            append_expr(output, ir_.parenthesized_exprs_.operand[index], context);
            output += ')';
            break;
        case ExprKind::template_id:
            append_expr(output, ir_.template_id_exprs_.callee[index], context, 12);
            output += '<';
            append_arguments(ir_.template_id_exprs_.arguments[index]);
            output += '>';
            break;
        case ExprKind::named:
            output += ir_.strings.view(ir_.named_exprs_.spelling[index]);
            break;
        case ExprKind::literal:
            output += ir_.strings.view(ir_.literal_exprs_.spelling[index]);
            break;
        case ExprKind::string_literal:
            output += '"';
            append_escaped_cpp_string(output, ir_.strings.view(ir_.string_literal_exprs_[index]));
            output += '"';
            break;
        case ExprKind::raw:
            output += ir_.strings.view(ir_.raw_exprs_.text[index]);
            break;
        case ExprKind::call:
            append_expr(output, ir_.call_exprs_.callee[index], context, 12);
            output += '(';
            append_arguments(ir_.call_exprs_.arguments[index]);
            output += ')';
            break;
        case ExprKind::member_access:
            append_expr(output, ir_.member_exprs_.object[index], context, 12);
            output += ir_.member_exprs_.through_pointer[index] ? "->" : ".";
            output += ir_.strings.view(ir_.member_exprs_.member[index]);
            break;
        case ExprKind::subscript:
            append_expr(output, ir_.subscript_exprs_.object[index], context, 12);
            output += '[';
            append_expr(output, ir_.subscript_exprs_.index[index], context, 1);
            output += ']';
            break;
        case ExprKind::binary: {
            auto const operation{ir_.binary_exprs_.operation[index]};
            auto append_operand = [&](Expr operand, bool right) {
                auto const conjunction{
                    operation == BinaryOperator::logical_or && operand.kind() == ExprKind::binary &&
                    ir_.binary_exprs_.operation[operand.index()] == BinaryOperator::logical_and};
                append_expr(output,
                            operand,
                            context,
                            conjunction ? expr_precedence(operand) : precedence,
                            right || conjunction);
            };
            append_operand(ir_.binary_exprs_.left[index], false);
            output += expr_detail::binary_spelling(operation);
            if (ir_.binary_exprs_.line_break_after[index]) {
                output.pop_back();
                output += "\n    ";
            }
            append_operand(ir_.binary_exprs_.right[index], true);
            break;
        }
        case ExprKind::unary:
            switch (ir_.unary_exprs_.operation[index]) {
                case UnaryOperator::logical_not:
                    output += '!';
                    break;
                case UnaryOperator::dereference:
                    output += '*';
                    break;
                case UnaryOperator::address_of:
                    output += '&';
                    break;
                case UnaryOperator::preincrement:
                    output += "++";
                    break;
                case UnaryOperator::bitwise_not:
                    output += '~';
                    break;
                case UnaryOperator::negate:
                    output += '-';
                    break;
            }
            append_expr(output, ir_.unary_exprs_.operand[index], context, 11, true);
            break;
        case ExprKind::static_cast_:
            output += "static_cast<";
            append_type(output, ir_.cast_exprs_.type[index]);
            output += ">(";
            append_expr(output, ir_.cast_exprs_.operand[index], context, 1);
            output += ')';
            break;
        case ExprKind::sizeof_type:
            output += "sizeof(";
            append_type(output, ir_.sizeof_exprs_.type[index]);
            output += ')';
            break;
        case ExprKind::initializer_list:
            if (auto const type{ir_.initializer_exprs_.type[index]}) {
                append_type(output, *type);
            }
            output += '{';
            append_arguments(ir_.initializer_exprs_.elements[index]);
            output += '}';
            break;
        case ExprKind::invalid:
            std::unreachable();
    }
    if (parenthesize) {
        output += ')';
    }
}
void CppRenderer::append_template(std::string& output,
                                  TemplateId declaration,
                                  RenderContext const& context) {
    assert(declaration);
    auto const row{declaration.index()};
    output += "template <";
    if (declaration.kind() == TemplateKind::authored_raw) {
        output += ir_.strings.view(ir_.templates_.authored_text[row]);
        state_.environments.mark_unknown(state_.current);
    } else if (declaration.kind() == TemplateKind::parameters) {
        auto const range{ir_.templates_.parameters[row]};
        auto const end{range.begin + range.count};
        for (auto position{range.begin}; position < end; ++position) {
            if (position != range.begin) {
                output += ", ";
            }
            auto const parameter{ir_.template_parameter_references_[position]};
            auto const index{parameter.index()};
            switch (parameter.kind()) {
                case TemplateParameterKind::type: {
                    output += "typename";
                    if (ir_.type_template_parameters_.pack[index]) {
                        output += "...";
                    }
                    auto const name{ir_.type_template_parameters_.name[index]};
                    if (name.value != 0) {
                        output += ' ';
                        output += ir_.strings.view(name);
                    }
                    auto const default_type{ir_.type_template_parameters_.default_type[index]};
                    if (default_type.value != 0) {
                        output += " = ";
                        append_type(output, default_type);
                    }
                    break;
                }
                case TemplateParameterKind::value: {
                    append_type(output, ir_.value_template_parameters_.type[index]);
                    if (ir_.value_template_parameters_.pack[index]) {
                        output += "...";
                    }
                    auto const name{ir_.value_template_parameters_.name[index]};
                    if (name.value != 0) {
                        output += ' ';
                        output += ir_.strings.view(name);
                    }
                    if (auto const value{ir_.value_template_parameters_.default_value[index]}) {
                        output += " = ";
                        append_expr(output, value, context);
                    }
                    break;
                }
            }
            auto const name{parameter.kind() == TemplateParameterKind::type
                                ? ir_.type_template_parameters_.name[index]
                                : ir_.value_template_parameters_.name[index]};
            if (name.value != 0) {
                state_.environments.declare(state_.current, name, CppDeclarationId{parameter});
            }
        }
    }
    output += '>';
}
auto CppRenderer::signature(std::uint32_t index,
                            RenderContext const& context,
                            EnvironmentId& environment) -> std::string {
    auto const id{ir_.function_nodes_.function[index].value};
    auto const declaration{ir_.function_nodes_.declaration[index] != 0};
    auto const header{ir_.function_nodes_.is_header[index] != 0};
    auto const owner{ir_.function_nodes_.owner[index]};
    auto const format{ir_.functions_.formatting[id]};
    auto const implementation{ir_.functions_.implementation[id]};
    if (owner && implementation.kind() == FunctionImplementationKind::deleted) {
        throw std::invalid_argument{"Function '" + expression(ir_.functions_.name[id], context) +
                                    "' deleted function cannot be an out-of-line definition"};
    }
    auto const return_syntax{
        ir_.function_nodes_.return_syntax[index].value_or(ir_.functions_.return_syntax[id])};
    auto const receiver{ir_.functions_.receiver[id]};
    std::string output;
    if (ir_.functions_.flags[id].is_nodiscard()) {
        output += "[[nodiscard]] ";
    }
    auto& state{state_};
    EnvironmentGuard const guard{state.current, state.current};
    auto const enclosing{state.current};
    auto const template_environment{
        state.environments.append(enclosing, EnvironmentKind::template_)};
    state.current = template_environment;
    std::string template_text;
    if (auto const declaration{ir_.functions_.template_declaration[id]}) {
        append_template(template_text, declaration, context);
    }
    if (ir_.functions_.flags[id].is_inline() && header && (owner || !in_record_scope(enclosing))) {
        output += "inline ";
    }
    if (receiver == FunctionReceiver::static_ && !owner) {
        output += "static ";
    }
    if (ir_.functions_.flags[id].is_constexpr()) {
        output += "constexpr ";
    }
    auto const export_specifier{(declaration || header) ? ir_.functions_.export_specifier[id]
                                                        : StringId{}};
    if (export_specifier.value != 0 && !format.export_after_return_type) {
        output += ir_.strings.view(export_specifier);
        output += ' ';
    }
    if (ir_.functions_.kind[id] == FunctionKind::ordinary) {
        if (return_syntax == ReturnSyntax::trailing) {
            output += "auto";
        } else {
            append_type(output, ir_.functions_.return_type[id]);
        }
        output += ' ';
    } else if (ir_.functions_.kind[id] == FunctionKind::explicit_constructor && !owner) {
        output += "explicit ";
    }
    if (export_specifier.value != 0 && format.export_after_return_type) {
        output += ir_.strings.view(export_specifier);
        output += ' ';
    }
    if (owner) {
        append_type(output, *owner);
        output += "::";
    }
    append_expr(output, ir_.functions_.name[id], context);
    state.current = enclosing;
    if (owner) {
        state.current = record_environment(ir_.types.spelling(*owner));
    }
    auto const function_environment{
        state.environments.append(state.current, EnvironmentKind::function)};
    state.environments.import_symbols(function_environment, template_environment);
    state.current = function_environment;
    environment = function_environment;
    output += '(';
    auto const parameters{ir_.functions_.parameters[id]};
    auto const parameter_end{parameters.begin + parameters.count};
    for (auto p{parameters.begin}; p < parameter_end; ++p) {
        if (p != parameters.begin) {
            output += ", ";
        }
        append_type(output, ir_.parameters_.type[ir_.parameter_references_[p].value]);
        auto const parameter{ir_.parameter_references_[p]};
        auto const parameter_name{ir_.strings.intern(ir_.parameters_.name[parameter.value])};
        if (parameter_name.value != 0) {
            output += ' ';
            output += ir_.strings.view(parameter_name);
            state.environments.declare(state.current, parameter_name, CppDeclarationId{parameter});
        }
        if (auto const value{ir_.parameters_.default_value[ir_.parameter_references_[p].value]};
            value && (declaration || header)) {
            output += " = ";
            output += ir_.strings.view(*value);
        }
    }
    output += ')';
    switch (receiver) {
        case FunctionReceiver::unqualified:
        case FunctionReceiver::static_:
            break;
        case FunctionReceiver::const_:
            output += " const";
            break;
        case FunctionReceiver::lvalue:
            output += " &";
            break;
        case FunctionReceiver::const_lvalue:
            output += " const &";
            break;
        case FunctionReceiver::rvalue:
            output += " &&";
            break;
        case FunctionReceiver::const_rvalue:
            output += " const &&";
            break;
    }
    auto const exception{ir_.functions_.exception_specification[id]};
    switch (exception.kind()) {
        case FunctionExceptionKind::potentially_throwing:
            break;
        case FunctionExceptionKind::noexcept_:
            output += " noexcept";
            break;
        case FunctionExceptionKind::conditional:
            output += " noexcept(";
            append_expr(output, ir_.conditional_noexcept_[exception.index()], context);
            output += ')';
            break;
    }
    if (ir_.functions_.kind[id] == FunctionKind::ordinary &&
        return_syntax == ReturnSyntax::trailing) {
        output += format.trailing_return_placement == FunctionTrailingReturnPlacement::next_line
                    ? "\n    -> "
                    : " -> ";
        append_type(output, ir_.functions_.return_type[id]);
    }
    if (auto const clause{ir_.functions_.requires_clause[id]}) {
        auto const value{expression(clause, context)};
        switch (format.requires_placement) {
            case FunctionRequiresPlacement::trailing_next_line:
                output += "\n    requires ";
                output += value;
                break;
            case FunctionRequiresPlacement::trailing_same_line:
                output += " requires ";
                output += value;
                break;
            case FunctionRequiresPlacement::before_signature:
                output.insert(0, "    requires " + std::string{value} + "\n");
                break;
        }
    }
    if (!declaration && implementation.kind() == FunctionImplementationKind::body) {
        auto const initializers{ir_.function_bodies_.initializers[implementation.index()]};
        auto const end{initializers.begin + initializers.count};
        for (auto m{initializers.begin}; m < end; ++m) {
            output += m == initializers.begin ? "\n    : " : "\n    , ";
            output +=
                ir_.strings.view(ir_.initializers_.member[ir_.initializer_references_[m].value]);
            output += '{';
            append_expr(
                output, ir_.initializers_.value[ir_.initializer_references_[m].value], context);
            output += '}';
        }
    }
    if (!template_text.empty()) {
        template_text +=
            format.template_placement == FunctionTemplatePlacement::same_line ? " " : "\n";
        output.insert(0, template_text);
    }
    if (ioj::is_any(implementation.kind(),
                    {FunctionImplementationKind::defaulted, FunctionImplementationKind::deleted})) {
        output += format.disposition_placement == FunctionDispositionPlacement::next_line ? "\n= "
                                                                                          : " = ";
        output +=
            implementation.kind() == FunctionImplementationKind::defaulted ? "default" : "delete";
    }
    return output;
}
void CppRenderer::append_node(std::string& output, NodeId node, RenderContext const& context) {
    auto& state{state_};
    EnvironmentGuard const scope_guard{state.current, state.current};
    auto const enclosing{state.current};
    auto const i{node.index()};
    auto indent = [&] { append_indent(output, context); };
    auto write = [&](std::string_view text) { append_indented(output, text, context); };
    auto close = [&](std::string_view text) {
        output += '\n';
        indent();
        output += text;
    };
    auto type = [&](CppTypeId id) { append_type(output, id); };
    auto text = [&](StringId id) { output += ir_.strings.view(id); };
    auto expr = [&](Expr id) { append_expr(output, id, context); };
    auto scoped_nodes = [&](ir::Range children) {
        EnvironmentGuard const guard{state.current, block_environment(ir_.nodes(children))};
        append_nodes(output, ir_.nodes(children), context.indent(), 1);
    };
    auto template_prefix = [&](TemplateId declaration) {
        if (declaration) {
            state.current = state.environments.append(state.current, EnvironmentKind::template_);
            indent();
            append_template(output, declaration, context);
            output += '\n';
        }
    };
    auto include = [&](std::string_view path, bool system) {
        indent();
        output += "#include ";
        output += system ? '<' : '"';
        output += path;
        output += system ? '>' : '"';
    };
    auto initialize = [&](Expr value) {
        auto const direct_list{value.kind() == ExprKind::initializer_list &&
                               !ir_.initializer_exprs_.type[value.index()]};
        if (!direct_list) {
            output += '{';
        }
        expr(value);
        if (!direct_list) {
            output += '}';
        }
    };
    auto member = [&](auto const& members, std::string_view prefix) {
        indent();
        output += prefix;
        type(members.type[i]);
        output += ' ';
        output += ir_.strings.view(members.name[i]);
        if (auto const initializer{members.initializer[i]}) {
            initialize(initializer);
        }
        output += ';';
    };
    switch (node.kind()) {
        case NodeKind::raw:
            write(ir_.strings.view(ir_.raw_nodes_.text[i]));
            break;
        case NodeKind::raw_lines: {
            auto const range{ir_.raw_lines_nodes_.lines[i]};
            auto const end{range.begin + range.count};
            for (auto line{range.begin}; line < end; ++line) {
                if (line != range.begin) {
                    output += '\n';
                }
                write(ir_.strings.view(ir_.raw_lines_[line]));
            }
            break;
        }
        case NodeKind::line_comment:
            write("// " + std::string{ir_.strings.view(ir_.line_comment_nodes_.text[i])});
            break;
        case NodeKind::line_directive:
            std::format_to(
                std::back_inserter(output), "#line {} \"", ir_.line_directive_nodes_.line[i]);
            append_escaped_cpp_string(output, ir_.strings.view(ir_.line_directive_nodes_.path[i]));
            output += '"';
            break;
        case NodeKind::block_comment:
            write("/* " + std::string{ir_.strings.view(ir_.block_comment_nodes_.text[i])} + " */");
            break;
        case NodeKind::block:
            write("{");
            output += '\n';
            state.current = block_environment(ir_.nodes(ir_.block_nodes_.children[i]));
            append_nodes(output, ir_.nodes(ir_.block_nodes_.children[i]), context.indent(), 1);
            close("}");
            break;
        case NodeKind::if_stmt:
            indent();
            output += "if (";
            expr(ir_.if_stmt_nodes_.condition[i]);
            output += ") {\n";
            scoped_nodes(ir_.if_stmt_nodes_.then_children[i]);
            close("}");
            if (auto const otherwise{ir_.if_stmt_nodes_.else_children[i]}) {
                output += " else {\n";
                scoped_nodes(*otherwise);
                close("}");
            }
            break;
        case NodeKind::for_stmt: {
            auto const initializer{ir_.for_stmt_nodes_.initializer[i]};
            state.current = block_environment(std::span{&initializer, 1});
            indent();
            output += "for (";
            auto initializer_context{context};
            initializer_context.indent_level = 0;
            append_node(output, initializer, initializer_context);
            output += ' ';
            expr(ir_.for_stmt_nodes_.condition[i]);
            output += "; ";
            expr(ir_.for_stmt_nodes_.increment[i]);
            output += ") {\n";
            scoped_nodes(ir_.for_stmt_nodes_.children[i]);
            close("}");
            break;
        }
        case NodeKind::switch_stmt: {
            indent();
            output += "switch (";
            expr(ir_.switch_stmt_nodes_.condition[i]);
            output += ") {";
            auto const range{ir_.switch_stmt_nodes_.cases[i]};
            auto const end{range.begin + range.count};
            for (auto c{range.begin}; c < end; ++c) {
                output += '\n';
                indent();
                if (auto const label{ir_.cases_.label[ir_.case_references_[c].value]}) {
                    output += "case ";
                    expr(*label);
                } else {
                    output += "default";
                }
                auto const row{ir_.case_references_[c].value};
                auto const braced{ir_.cases_.braced[row]};
                output += braced ? ": {\n" : ":\n";
                scoped_nodes(ir_.cases_.body[row]);
                if (braced) {
                    close("}");
                }
            }
            close("}");
            break;
        }
        case NodeKind::break_stmt:
            write("break;");
            break;
        case NodeKind::expr_stmt:
            indent();
            expr(ir_.expr_stmt_nodes_.expr[i]);
            output += ';';
            break;
        case NodeKind::return_stmt:
            indent();
            output += "return";
            if (auto const value{ir_.return_stmt_nodes_.expr[i]}) {
                output += ' ';
                expr(*value);
            }
            output += ';';
            break;
        case NodeKind::assignment_stmt:
            indent();
            expr(ir_.assignment_stmt_nodes_.target[i]);
            output += " = ";
            expr(ir_.assignment_stmt_nodes_.value[i]);
            output += ';';
            break;
        case NodeKind::variable_declaration_stmt:
            indent();
            type(ir_.variable_declaration_stmt_nodes_.type[i]);
            output += ' ';
            text(ir_.variable_declaration_stmt_nodes_.name[i]);
            state.environments.declare(
                state.current,
                ir_.strings.intern(ir_.variable_declaration_stmt_nodes_.name[i]),
                CppDeclarationId{node});
            if (auto const initializer{ir_.variable_declaration_stmt_nodes_.initializer[i]}) {
                initialize(*initializer);
            } else {
                output += "{}";
            }
            output += ';';
            break;
        case NodeKind::static_assert_:
            indent();
            output += "static_assert(";
            expr(ir_.static_assert_nodes_.condition[i]);
            if (auto const message{ir_.strings.view(ir_.static_assert_nodes_.message[i])};
                !message.empty()) {
                output += ", \"";
                output += escape_cpp_string(message);
                output += '"';
            }
            output += ");";
            break;
        case NodeKind::new_lines:
            output.append(static_cast<std::size_t>(ir_.new_lines_nodes_.count[i]), '\n');
            break;
        case NodeKind::access_specifier: {
            auto const begin{output.size()};
            indent();
            if (ir_.access_specifier_nodes_.indentation[i] == AccessIndentation::outdented &&
                output.size() - begin >= 2) {
                output.resize(output.size() - 2);
            }
            switch (ir_.access_specifier_nodes_.access[i]) {
                case Access::public_:
                    output += "public:";
                    break;
                case Access::protected_:
                    output += "protected:";
                    break;
                case Access::private_:
                    output += "private:";
                    break;
            }
            break;
        }
        case NodeKind::include: {
            auto const path{ir_.strings.view(ir_.include_nodes_.path[i])};
            include(path, include_is_system(path, ir_.include_nodes_.style[i]));
            break;
        }
        case NodeKind::include_dependencies: {
            bool first_group{true};
            for (auto const& group : context.include_groups) {
                if (!first_group) {
                    output += "\n\n";
                }
                bool first{true};
                for (auto const& path : group) {
                    if (!first) {
                        output += '\n';
                    }
                    include(path, include_is_system(path));
                    first = false;
                }
                first_group = false;
            }
            break;
        }
        case NodeKind::forward_declaration:
            template_prefix(ir_.forward_declaration_nodes_.template_declaration[i]);
            indent();
            output += to_serialized_string(ir_.forward_declaration_nodes_.kind[i]);
            output += ' ';
            text(ir_.forward_declaration_nodes_.name[i]);
            output += ';';
            break;
        case NodeKind::friend_declaration:
            template_prefix(ir_.friend_declaration_nodes_.template_declaration[i]);
            indent();
            output += "friend ";
            if (auto const friend_type{ir_.friend_declaration_nodes_.type[i]};
                friend_type.value != 0) {
                type(friend_type);
            } else {
                output += to_serialized_string(ir_.friend_declaration_nodes_.kind[i]);
                output += ' ';
                text(ir_.friend_declaration_nodes_.name[i]);
            }
            output += ';';
            break;
        case NodeKind::using_declaration:
            if (ir_.using_declaration_nodes_.name[i].value == 0) {
                auto const imported{ir_.using_declaration_nodes_.type[i]};
                auto const shape{ir_.types.shapes_[imported.value - 1]};
                auto const components{std::span{ir_.types.name_components_}.subspan(
                    shape.parts.begin, shape.parts.count)};
                auto const target{state_.environments.reference(components)};
                if (target.declaring_scope == state_.current) {
                    break;
                }
                indent();
                output += "using ";
                append_name(output, components.first(components.size() - 1));
                output += "::";
                text(components.back());
                output += ';';
                break;
            }
            template_prefix(ir_.using_declaration_nodes_.template_declaration[i]);
            indent();
            output += "using ";
            text(ir_.using_declaration_nodes_.name[i]);
            output += " = ";
            type(ir_.using_declaration_nodes_.type[i]);
            output += ';';
            break;
        case NodeKind::instance_member:
            member(ir_.instance_members_, "");
            break;
        case NodeKind::static_member: {
            template_prefix(ir_.static_members_.template_declaration[i]);
            member(ir_.static_members_,
                   ir_.static_members_.is_inline[i] ? "inline static " : "static ");
            break;
        }
        case NodeKind::constexpr_member: {
            template_prefix(ir_.constexpr_members_.template_declaration[i]);
            member(ir_.constexpr_members_,
                   ir_.constexpr_members_.is_inline[i] ? "inline static constexpr "
                                                       : "static constexpr ");
            break;
        }
        case NodeKind::function: {
            auto const id{ir_.function_nodes_.function[i].value};
            EnvironmentId function_environment;
            write(signature(i, context, function_environment));
            auto const implementation{ir_.functions_.implementation[id]};
            if (ir_.function_nodes_.declaration[i] ||
                implementation.kind() != FunctionImplementationKind::body) {
                output += ';';
                break;
            }
            auto const body{
                ir_.block_nodes_
                    .children[ir_.function_bodies_.block[implementation.index()].index()]};
            state.current = function_environment;
            state.current = block_environment(ir_.nodes(body));
            auto const format{ir_.functions_.formatting[id]};
            if (format.body_layout == FunctionBodyLayout::compact) {
                auto body_context{context};
                body_context.indent_level = 0;
                output += " { ";
                append_nodes(output, ir_.nodes(body), body_context, 1);
                output += " }";
            } else {
                if (format.opening_brace_placement ==
                    FunctionOpeningBracePlacement::separate_line) {
                    close("{");
                } else {
                    output += " {";
                }
                output += '\n';
                append_nodes(output, ir_.nodes(body), context.indent(), 1);
                close("}");
            }
            break;
        }
        case NodeKind::enum_: {
            indent();
            output += "enum";
            if (ir_.enum_nodes_.scoped[i]) {
                output += " class";
            }
            if (auto const specifier{ir_.enum_nodes_.export_specifier[i]}; specifier.value != 0) {
                output += ' ';
                output += ir_.strings.view(specifier);
            }
            output += ' ';
            text(ir_.enum_nodes_.name[i]);
            if (auto const underlying{ir_.enum_nodes_.underlying_type[i]};
                underlying.value != 0 && !ir_.types.spelling(underlying).empty()) {
                output += " : ";
                type(underlying);
            }
            output += " {\n";
            auto const range{ir_.enum_nodes_.values[i]};
            auto const end{range.begin + range.count};
            for (auto e{range.begin}; e < end; ++e) {
                append_indented(
                    output,
                    ir_.strings.view(ir_.enumerators_.name[ir_.enumerator_references_[e].value]),
                    context.indent());
                if (auto const initializer{
                        ir_.enumerators_.initializer[ir_.enumerator_references_[e].value]}) {
                    output += " = ";
                    text(*initializer);
                }
                if (auto const annotation{
                        ir_.enumerators_.annotation[ir_.enumerator_references_[e].value]}) {
                    output += ' ';
                    text(*annotation);
                }
                output += ',';
                if (e + 1 < end) {
                    output += '\n';
                }
            }
            close("};");
            break;
        }
        case NodeKind::struct_: {
            if (auto const parameters{ir_.struct_nodes_.template_declaration[i]}) {
                template_prefix(parameters);
                if (auto const clause{ir_.struct_nodes_.requires_clause[i]}) {
                    indent();
                    output += "    requires ";
                    expr(clause);
                    output += '\n';
                }
            }
            indent();
            output += to_serialized_string(ir_.struct_nodes_.record_kind[i]);
            if (auto const specifier{ir_.struct_nodes_.export_specifier[i]}; specifier.value != 0) {
                output += ' ';
                output += ir_.strings.view(specifier);
            }
            output += ' ';
            expr(ir_.struct_nodes_.name[i]);
            bool first{true};
            for (auto const base : ir_.type_list(ir_.struct_nodes_.bases[i])) {
                output += first ? " : " : ", ";
                type(base);
                first = false;
            }
            auto member_context{context.indent()};
            state.current = state.node_environments.at(RenderState::node_key(node, enclosing));
            output += " {\n";
            append_nodes(output, ir_.nodes(ir_.struct_nodes_.children[i]), member_context, 2);
            close("};");
            break;
        }
        case NodeKind::namespace_: {
            std::string const name{ir_.strings.view(ir_.namespace_nodes_.name[i])};
            indent();
            output += "namespace ";
            output += name;
            output += name.empty() ? "{\n" : " {\n";
            state.current = state.node_environments.at(RenderState::node_key(node, enclosing));
            append_nodes(output, ir_.nodes(ir_.namespace_nodes_.children[i]), context, 2);
            close("} // namespace");
            if (!name.empty()) {
                output += ' ';
                output += name;
            }
            break;
        }
        case NodeKind::invalid:
            std::unreachable();
    }
}
void CppRenderer::append_nodes(std::string& output,
                               std::span<NodeId const> values,
                               RenderContext const& context,
                               int newlines) {
    auto const start{output.size()};
    for (auto const node : values) {
        if (node.kind() == NodeKind::new_lines) {
            while (output.size() > start && output.back() == '\n') {
                output.pop_back();
            }
            output.append(static_cast<std::size_t>(ir_.new_lines_nodes_.count[node.index()]), '\n');
            continue;
        }
        auto const before_separator{output.size()};
        if (output.size() != start && output.back() != '\n') {
            output.append(static_cast<std::size_t>(newlines), '\n');
        }
        auto const before_node{output.size()};
        append_node(output, node, context);
        if (output.size() == before_node) {
            output.resize(before_separator);
        }
    }
}

} // namespace lispb::codegen
